#include <imgui.h> // Has to be included before reshade.hpp
#include <reshade.hpp>

// DirectX
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>

#include <MinHook.h>

// std
#include <filesystem>
#include <vector>
#include <array>
#include <fstream>

#include "ComPtr.h"
#include "Ensure.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0

enum UPSCALER
{
	UPSCALER_SGSR1,
	UPSCALER_AMD_FFX_FSR1_EASU,
	UPSCALER_CATMULL_ROM,
	UPSCALER_POWER_OF_GARAMOND
};
static int g_upscaler = UPSCALER_SGSR1;

// AnyUpscale folder path.
static std::filesystem::path g_any_upscale_path;

static HINSTANCE g_instance;

// Our window.
static ATOM g_window_class;
static HWND g_hwnd;
static int g_window_width;
static int g_window_height;

// Game's resoulution, game's back buffer dimensions.
static int g_src_width = 1280;
static int g_src_height = 720;

// Scaled dimensions, our output viewport dimensions.
static int g_dst_width;
static int g_dst_height;

// Game's device.
static Com_ptr<ID3D11Device> g_device;

// Game's imidiate context.
static Com_ptr<ID3D11DeviceContext> g_ctx;

// Game's back buffer SRV.
static Com_ptr<ID3D11ShaderResourceView> g_srv_src_back_buffer;

// Our swapchain
static Com_ptr<IDXGISwapChain1> g_swapchain;

// Our back buffer RTV.
static Com_ptr<ID3D11RenderTargetView> g_rtv_dst_back_buffer;

// Upscaled resources, output of upscalers.
static Com_ptr<ID3D11RenderTargetView> g_rtv_upscaled;
static Com_ptr<ID3D11UnorderedAccessView> g_uav_upscaled;
static Com_ptr<ID3D11ShaderResourceView> g_srv_upscaled;

static Com_ptr<ID3D11VertexShader> g_vs_fullscreen_triangle;
static Com_ptr<ID3D11SamplerState> g_smp_linear;
static Com_ptr<ID3D11ComputeShader> g_cs_amd_ffx_fsr1_easu;
static Com_ptr<ID3D11PixelShader> g_ps_catmull_rom;

// SGSR1.
//

static Com_ptr<ID3D11PixelShader> g_ps_sgsr1;

enum SGSR_TARGET
{
	SGSR_TARGET_HIGH_QUALITY,
	SGSR_TARGET_MOBILE
};
static int g_sgsr_target = SGSR_TARGET_HIGH_QUALITY;

//

// Power of Garamond.
static Com_ptr<ID3D11PixelShader> g_ps_power_of_garamond_x;
static Com_ptr<ID3D11PixelShader> g_ps_power_of_garamond_y;
static Com_ptr<ID3D11RenderTargetView> g_rtv_power_of_garamond_x;	
static Com_ptr<ID3D11ShaderResourceView> g_srv_power_of_garamond_x;
static float g_power_of_garamond_blur = 1.0;
static float g_power_of_garamond_support = 2.0;
static float g_power_of_garamond_n = 2.0;
static float g_power_of_garamond_m = 1.0;

// AMD FFX RCAS.
static Com_ptr<ID3D11PixelShader> g_ps_amd_ffx_rcas;
static float g_amd_ffx_rcas_sharpness = 0.87f;

constexpr float g_clear_color[4] = {};
static Com_ptr<ID3D11RasterizerState> g_rasterizer;
static float g_viewport_offset_x;
static float g_viewport_offset_y;
static float g_mip_lod_bias;

typedef HRESULT(__stdcall *IDXGISwapChain__Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static IDXGISwapChain__Present g_original_present;

// Wrappers for reshade::log::message
//

template<typename... Args>
void log_info(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::info, msg.c_str());
}

template<typename... Args>
void log_error(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::error, msg.c_str());
}

template<typename... Args>
void log_debug(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::debug, msg.c_str());
}

//

static std::filesystem::path get_any_upscale_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\AnyUpscale";
	return path;
}

static void set_dims()
{
	// Set window dimensions to the virtual screen resolution.
	g_window_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	g_window_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	// Calculate destination dimensions while keeping the source aspect ratio.
	const double src_aspect = (double)g_src_width / (double)g_src_height;
	const double window_aspect = (double)g_window_width / (double)g_window_height;
	const double scale = window_aspect > src_aspect ? (double)g_window_height / (double)g_src_height : (double)g_window_width / (double)g_src_width;
	g_dst_width = std::ceil((double)g_src_width * scale);
	g_dst_height = std::ceil((double)g_src_height * scale);

	// Calculate the output viewport offset.
	g_viewport_offset_x = (float)(g_window_width - g_dst_width) * 0.5f;
	g_viewport_offset_y = (float)(g_window_height - g_dst_height) * 0.5f;

	// As recommended for AMD FFX FSR1.
	g_mip_lod_bias = -std::log2(scale);
}

static void compile_shader(ID3DBlob** code, const wchar_t* file, const char* target, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	std::filesystem::path path = g_any_upscale_path;
	path /= file;
	Com_ptr<ID3DBlob> error;
	auto hr = D3DCompileFromFile(path.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code, &error);
	if (FAILED(hr)) {
		if (error) {
			log_error("D3DCompileFromFile: {}", (const char*)error->GetBufferPointer());
		}
		else {
			log_error("D3DCompileFromFile: HRESULT {:08X}", hr);
		}
	}

	#if DEV && OUTPUT_ASSEMBLY
	Com_ptr<ID3DBlob> disassembly;
	D3DDisassemble((*code)->GetBufferPointer(), (*code)->GetBufferSize(), 0, nullptr, &disassembly);
	std::ofstream assembly(path.replace_extension("asm"));
	assembly.write((const char*)disassembly->GetBufferPointer(), disassembly->GetBufferSize());
	#endif
}

static void create_compute_shader(ID3D11ComputeShader** cs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "cs_5_0", entry_point, defines);
	ensure(g_device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, cs), >= 0);
}

static void create_vertex_shader(ID3D11VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "vs_5_0", entry_point, defines);
	ensure(g_device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vs), >= 0);
}

static void create_pixel_shader(ID3D11PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "ps_5_0", entry_point, defines);
	ensure(g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, ps), >= 0);
}

// Not thread safe, should we care?
static void create_resources(bool reset_all = false)
{
	if (reset_all) {
		g_ps_sgsr1.reset();
		g_cs_amd_ffx_fsr1_easu.reset();
		g_ps_catmull_rom.reset();
		g_ps_power_of_garamond_x.reset();
		g_rtv_power_of_garamond_x.reset();
		g_srv_power_of_garamond_x.reset();
		g_ps_power_of_garamond_y.reset();
		g_ps_amd_ffx_rcas.reset();
		g_uav_upscaled.reset();
		g_rtv_upscaled.reset();
		g_srv_upscaled.reset();
		g_rtv_dst_back_buffer.reset();
	}

	// This is shared by most upscale methods.
	D3D11_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = g_dst_width;
	tex_desc.Height = g_dst_height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	tex_desc.SampleDesc.Count = 1;

	switch(g_upscaler) {
		case UPSCALER_SGSR1: {
			// Create SGSR1 PS.
			if (!g_ps_sgsr1) {
				const std::string input_size_in_pixels_x_str = std::to_string((float)g_src_width);
				const std::string input_size_in_pixels_y_str = std::to_string((float)g_src_height);
				std::string sgsr_target_str = "";
				if (g_sgsr_target == SGSR_TARGET_HIGH_QUALITY) {
					sgsr_target_str = "SGSR_TARGET_HIGH_QUALITY";
				}
				else if (g_sgsr_target == SGSR_TARGET_MOBILE) {
					sgsr_target_str = "SGSR_TARGET_MOBILE";
				}
				const D3D_SHADER_MACRO defines[] = {
					{ "INPUT_SIZE_IN_PIXELS_X", input_size_in_pixels_x_str.c_str() },
					{ "INPUT_SIZE_IN_PIXELS_Y", input_size_in_pixels_y_str.c_str() },
					{ sgsr_target_str.c_str(), nullptr },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_sgsr1, L"SGSR1_ps.hlsl", "main", defines);
			}

			// Create back buffer RTV.
			if (!g_rtv_dst_back_buffer) {
				Com_ptr<ID3D11Texture2D> tex;
				ensure(g_swapchain->GetBuffer(0, IID_PPV_ARGS(&tex)), >= 0);
				ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_dst_back_buffer), >= 0);
			}

			break;
		}
		case UPSCALER_AMD_FFX_FSR1_EASU: {
			// Create AMD FFX FSR1 EASU CS.
			if (!g_cs_amd_ffx_fsr1_easu) {
				const std::string input_viewport_in_pixels_x_str = std::to_string((float)g_src_width);
				const std::string input_viewport_in_pixels_y_str = std::to_string((float)g_src_height);
				const std::string input_size_in_pixels_x_str = std::to_string((float)g_src_width);
				const std::string input_size_in_pixels_y_str = std::to_string((float)g_src_height);
				const std::string output_size_in_pixels_x_str = std::to_string((float)g_dst_width);
				const std::string output_size_in_pixels_y_str = std::to_string((float)g_dst_height);
				const D3D_SHADER_MACRO defines[] = {
					{ "INPUT_VIEWPORT_IN_PIXELS_X", input_viewport_in_pixels_x_str.c_str() },
					{ "INPUT_VIEWPORT_IN_PIXELS_Y", input_viewport_in_pixels_y_str.c_str() },
					{ "INPUT_SIZE_IN_PIXELS_X", input_size_in_pixels_x_str.c_str() },
					{ "INPUT_SIZE_IN_PIXELS_Y", input_size_in_pixels_y_str.c_str() },
					{ "OUTPUT_SIZE_IN_PIXELS_X", output_size_in_pixels_x_str.c_str() },
					{ "OUTPUT_SIZE_IN_PIXELS_Y", output_size_in_pixels_y_str.c_str() },
					{ nullptr, nullptr }
				};
				create_compute_shader(&g_cs_amd_ffx_fsr1_easu, L"AMD_FFX_FSR1_EASU_cs.hlsl", "main", defines);
			}

			// Create AMD FFX RCAS PS.
			if (!g_ps_amd_ffx_rcas) {
				const std::string sharpness_str = std::to_string(g_amd_ffx_rcas_sharpness);
				const D3D_SHADER_MACRO defines[] = {
					{ "SHARPNESS", sharpness_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_amd_ffx_rcas, L"AMD_FFX_RCAS_ps.hlsl", "main", defines);
			}

			// Create upscaled UAV and SRV.
			if (!g_uav_upscaled && !g_srv_upscaled) {
				Com_ptr<ID3D11Texture2D> tex;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, &tex), >= 0);

				// Create the UAV.
				D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
				uav_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateUnorderedAccessView(tex.get(), &uav_desc, &g_uav_upscaled), >= 0);

				// Create the SRV.
				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
				srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_upscaled), >= 0);
			}

			// Create back buffer RTV.
			if (!g_rtv_dst_back_buffer) {
				Com_ptr<ID3D11Texture2D> tex;
				ensure(g_swapchain->GetBuffer(0, IID_PPV_ARGS(&tex)), >= 0);
				D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_dst_back_buffer), >= 0);
			}

			break;
		}
		case UPSCALER_CATMULL_ROM: {
			// Create Catmull-Rom PS.
			if (!g_ps_catmull_rom) {
				const std::string src_size_str = std::format("float2({},{})", g_src_width, g_src_height);
				const D3D_SHADER_MACRO defines[] = {
					{ "SRC_SIZE", src_size_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_catmull_rom, L"CatmullRom_ps.hlsl", "main", defines);
			}

			// Create AMD FFX RCAS PS.
			if (!g_ps_amd_ffx_rcas) {
				const std::string sharpness_str = std::to_string(g_amd_ffx_rcas_sharpness);
				const D3D_SHADER_MACRO defines[] = {
					{ "SHARPNESS", sharpness_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_amd_ffx_rcas, L"AMD_FFX_RCAS_ps.hlsl", "main", defines);
			}

			// Create upscaled RTV and SRV.
			if (!g_rtv_upscaled && !g_srv_upscaled) {
				Com_ptr<ID3D11Texture2D> tex;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, &tex), >= 0);

				// Create the RTV.
				D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_upscaled), >= 0);

				// Create the SRV.
				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
				srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_upscaled), >= 0);
			}

			// Create back buffer RTV.
			if (!g_rtv_dst_back_buffer) {
				Com_ptr<ID3D11Texture2D> tex;
				ensure(g_swapchain->GetBuffer(0, IID_PPV_ARGS(&tex)), >= 0);
				D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_dst_back_buffer), >= 0);
			}

			break;
		}
		case UPSCALER_POWER_OF_GARAMOND: {
			// Create Power of Garamond PSes.
			if (!g_ps_power_of_garamond_x && !g_ps_power_of_garamond_y) {
				const std::string src_size_str = std::format("float2({},{})", g_src_width, g_src_height);
				std::string axis_str = std::format("float2({},{})", 1.0f, 0.0f); // X axis.
				const std::string blur_str = std::to_string(g_power_of_garamond_blur);
				const std::string support_str = std::to_string(g_power_of_garamond_support);
				const std::string n_str = std::to_string(g_power_of_garamond_n);
				const std::string m_str = std::to_string(g_power_of_garamond_m);
				D3D_SHADER_MACRO defines[] = {
					{ "SRC_SIZE", src_size_str.c_str() },
					{ "AXIS", axis_str.c_str() },
					{ "BLUR", blur_str.c_str() },
					{ "SUPPORT", support_str.c_str() },
					{ "N", n_str.c_str() },
					{ "M", m_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_power_of_garamond_x, L"PowerOfGaramond_ps.hlsl", "main", defines);
				axis_str = std::format("float2({},{})", 0.0f, 1.0f); // Y axis.
				defines[1].Definition = axis_str.c_str();
				create_pixel_shader(&g_ps_power_of_garamond_y, L"PowerOfGaramond_ps.hlsl", "main", defines);
			}

			// Create AMD FFX RCAS PS.
			if (!g_ps_amd_ffx_rcas) {
				const std::string sharpness_str = std::to_string(g_amd_ffx_rcas_sharpness);
				const D3D_SHADER_MACRO defines[] = {
					{ "SHARPNESS", sharpness_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(&g_ps_amd_ffx_rcas, L"AMD_FFX_RCAS_ps.hlsl", "main", defines);
			}

			// Create Power of Garamond RTVs and SRVs.
			if (!g_rtv_upscaled && !g_srv_upscaled && !g_rtv_power_of_garamond_x && !g_srv_power_of_garamond_x) {
				Com_ptr<ID3D11Texture2D> tex;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, &tex), >= 0);

				// Create upscaled RTV.
				D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_upscaled), >= 0);

				// Create upscaled SRV.
				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
				srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_upscaled), >= 0);

				// Create pass in X axis RTV and SRV.
				tex_desc.Width = g_dst_width;
				tex_desc.Height = g_src_height;
				tex_desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
				ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_power_of_garamond_x), >= 0);
				ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_power_of_garamond_x), >= 0);
			}

			// Create back buffer RTV.
			if (!g_rtv_dst_back_buffer) {
				Com_ptr<ID3D11Texture2D> tex;
				ensure(g_swapchain->GetBuffer(0, IID_PPV_ARGS(&tex)), >= 0);
				D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_dst_back_buffer), >= 0);
			}

			break;
		}
	}
}

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	// Prevent infinite recursive calls.
	static bool lock = false;
	if (lock) {
		lock = false;
		return g_original_present(swapchain, sync_interval, flags);
	}

	// Backup states.
	// Here, only backup states that will be commonly overwritten.
	//

	// Primitive topology.
	D3D11_PRIMITIVE_TOPOLOGY primitive_topology_original;
	g_ctx->IAGetPrimitiveTopology(&primitive_topology_original);

	// VS.
	Com_ptr<ID3D11VertexShader> vs_original;
	g_ctx->VSGetShader(&vs_original, nullptr, nullptr);

	// PS.
	Com_ptr<ID3D11PixelShader> ps_original;
	g_ctx->PSGetShader(&ps_original, nullptr, nullptr);
	Com_ptr<ID3D11ShaderResourceView> srv_original;
	g_ctx->PSGetShaderResources(0, 1, &srv_original);

	// Viewports.
	UINT num_viewports;
	g_ctx->RSGetViewports(&num_viewports, nullptr);
	std::vector<D3D11_VIEWPORT> viewports_original(num_viewports);
	g_ctx->RSGetViewports(&num_viewports, viewports_original.data());

	// Rasterizer.
	Com_ptr<ID3D11RasterizerState> rasterizer_original;
	g_ctx->RSGetState(&rasterizer_original);

	// Blend.
	Com_ptr<ID3D11BlendState> blend_original;
	FLOAT blend_factor_original[4];
	UINT sample_mask_original;
	g_ctx->OMGetBlendState(&blend_original, blend_factor_original, &sample_mask_original);

	// RTs.
	// We should backup all of them, leaving as is for now.
	// This needs testing.
	Com_ptr<ID3D11RenderTargetView> rtv_original;
	Com_ptr<ID3D11DepthStencilView> dsv_original;
	g_ctx->OMGetRenderTargets(1, &rtv_original, &dsv_original);
	g_ctx->OMSetRenderTargets(0, nullptr, nullptr);

	//

	// Common bindings.
	g_ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
	g_ctx->RSSetState(g_rasterizer.get());
	g_ctx->OMSetBlendState(nullptr, nullptr, UINT_MAX);

	// Do upscaling.
	switch (g_upscaler) {
		case UPSCALER_SGSR1: {
			// Backup states.
			Com_ptr<ID3D11SamplerState> smp_original;
			g_ctx->PSGetSamplers(0, 1, &smp_original);

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = g_viewport_offset_x;
			viewport.TopLeftY = g_viewport_offset_y;
			viewport.Width = g_dst_width;
			viewport.Height = g_dst_height;

			// Bindings.
			g_ctx->PSSetShader(g_ps_sgsr1.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_src_back_buffer);
			g_ctx->PSSetSamplers(0, 1, &g_smp_linear);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_dst_back_buffer, nullptr);

			g_ctx->ClearRenderTargetView(g_rtv_dst_back_buffer.get(), g_clear_color);
			g_ctx->Draw(3, 0);

			// Restore states.
			g_ctx->PSSetSamplers(0, 1, &smp_original);

			break;
		}
		case UPSCALER_AMD_FFX_FSR1_EASU: {
			// Backup states.
			Com_ptr<ID3D11ComputeShader> cs_original;
			g_ctx->CSGetShader(&cs_original, nullptr, nullptr);
			Com_ptr<ID3D11ShaderResourceView> srv_original;
			g_ctx->CSGetShaderResources(0, 1, &srv_original);
			Com_ptr<ID3D11SamplerState> smp_original;
			g_ctx->CSGetSamplers(0, 1, &smp_original);
			Com_ptr<ID3D11UnorderedAccessView> uav_original;
			g_ctx->CSGetUnorderedAccessViews(0, 1, &uav_original);

			// AMD FFX FSR1 EASU
			//

			// Bindings.
			g_ctx->CSSetShader(g_cs_amd_ffx_fsr1_easu.get(), nullptr, 0);
			g_ctx->CSSetShaderResources(0, 1, &g_srv_src_back_buffer);
			g_ctx->CSSetSamplers(0, 1, &g_smp_linear);
			g_ctx->CSSetUnorderedAccessViews(0, 1, &g_uav_upscaled, nullptr);

			// This value is the image region dimension that each thread group of the FSR shader operates on.
			constexpr int thread_group_work_region_dim = 16;

			const int dispatch_x = (g_dst_width + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
			const int dispatch_y = (g_dst_height + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
			g_ctx->Dispatch(dispatch_x, dispatch_y, 1);
			
			// Restore original and unbind our UAV.
			g_ctx->CSSetUnorderedAccessViews(0, 1, &uav_original, nullptr);

			//

			// AMD FFX RCAS
			//

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = g_viewport_offset_x;
			viewport.TopLeftY = g_viewport_offset_y;
			viewport.Width = g_dst_width;
			viewport.Height = g_dst_height;

			// Bindings.
			g_ctx->PSSetShader(g_ps_amd_ffx_rcas.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_upscaled);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_dst_back_buffer, nullptr);

			g_ctx->ClearRenderTargetView(g_rtv_dst_back_buffer.get(), g_clear_color);
			g_ctx->Draw(3, 0);

			//

			// Restore states.
			g_ctx->CSSetShader(cs_original.get(), nullptr, 0);
			g_ctx->CSSetShaderResources(0, 1, &srv_original);
			g_ctx->CSSetSamplers(0, 1, &smp_original);

			break;
		}
		case UPSCALER_CATMULL_ROM: {
			// Backup states.
			Com_ptr<ID3D11SamplerState> smp_original;
			g_ctx->PSGetSamplers(0, 1, &smp_original);

			// Catmull-Rom
			//

			D3D11_VIEWPORT viewport = {};
			viewport.Width = g_dst_width;
			viewport.Height = g_dst_height;

			// Bindings.
			g_ctx->PSSetShader(g_ps_catmull_rom.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_src_back_buffer);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_upscaled, nullptr);

			g_ctx->Draw(3, 0);
			g_ctx->OMSetRenderTargets(0, nullptr, nullptr);

			//

			// AMD FFX RCAS
			//

			viewport.TopLeftX = g_viewport_offset_x;
			viewport.TopLeftY = g_viewport_offset_y;

			// Bindings.
			g_ctx->PSSetShader(g_ps_amd_ffx_rcas.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_upscaled);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_dst_back_buffer, nullptr);

			g_ctx->ClearRenderTargetView(g_rtv_dst_back_buffer.get(), g_clear_color);
			g_ctx->Draw(3, 0);

			//

			// Restore states.
			g_ctx->PSSetSamplers(0, 1, &smp_original);
			
			break;
		}
		case UPSCALER_POWER_OF_GARAMOND: {
			// Backup states.
			Com_ptr<ID3D11SamplerState> smp_original;
			g_ctx->PSGetSamplers(0, 1, &smp_original);

			// Power of Garamond pass in X axis.
			//

			D3D11_VIEWPORT viewport = {};
			viewport.Width = g_dst_width;
			viewport.Height = g_src_height;

			// Bindings.
			g_ctx->PSSetShader(g_ps_power_of_garamond_x.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_src_back_buffer);
			g_ctx->PSSetSamplers(0, 1, &g_smp_linear);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_power_of_garamond_x, nullptr);

			g_ctx->Draw(3, 0);
			g_ctx->OMSetRenderTargets(0, nullptr, nullptr);

			//

			// Power of Garamond pass in Y axis.
			//

			viewport.Width = g_dst_width;
			viewport.Height = g_dst_height;

			// Bindings.
			g_ctx->PSSetShader(g_ps_power_of_garamond_y.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_power_of_garamond_x);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_upscaled, nullptr);

			g_ctx->Draw(3, 0);
			g_ctx->OMSetRenderTargets(0, nullptr, nullptr);

			//

			// AMD FFX RCAS
			//

			viewport.TopLeftX = g_viewport_offset_x;
			viewport.TopLeftY = g_viewport_offset_y;

			// Bindings.
			g_ctx->PSSetShader(g_ps_amd_ffx_rcas.get(), nullptr, 0);
			g_ctx->PSSetShaderResources(0, 1, &g_srv_upscaled);
			g_ctx->RSSetViewports(1, &viewport);
			g_ctx->OMSetRenderTargets(1, &g_rtv_dst_back_buffer, nullptr);

			g_ctx->ClearRenderTargetView(g_rtv_dst_back_buffer.get(), g_clear_color);
			g_ctx->Draw(3, 0);

			//

			// Restore states.
			g_ctx->PSSetSamplers(0, 1, &smp_original);

			break;
		}
	}

	lock = true;
	g_swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

	// Restore states.
	g_ctx->IASetPrimitiveTopology(primitive_topology_original);
	g_ctx->VSSetShader(vs_original.get(), nullptr, 0);
	g_ctx->PSSetShader(ps_original.get(), nullptr, 0);
	g_ctx->PSSetShaderResources(0, 1, &srv_original);
	g_ctx->RSSetViewports(num_viewports, viewports_original.data());
	g_ctx->RSSetState(rasterizer_original.get());
	g_ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);
	g_ctx->OMSetRenderTargets(1, &rtv_original, dsv_original.get());

	// Return to game's present.
	return S_OK;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	desc.mip_lod_bias = g_mip_lod_bias;
	log_info("mip lod bias: {}", desc.mip_lod_bias);
	return true;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// on_create_swapchain gets called before buffers are resized.
	// We have to release our reference too.
	g_srv_src_back_buffer.reset();

	// We need non _srgb source.
	if (desc.back_buffer.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
		desc.back_buffer.texture.format = reshade::api::format::r8g8b8a8_unorm;
	}
	else if (desc.back_buffer.texture.format == reshade::api::format::b8g8r8a8_unorm_srgb) {
		desc.back_buffer.texture.format = reshade::api::format::b8g8r8a8_unorm;
	}

	desc.back_buffer.usage |= reshade::api::resource_usage::shader_resource;
	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();

	// Many games don't handle mouse currsor properly,
	// is this the best we can do about it?
	SetCapture((HWND)swapchain->get_hwnd());

	// Hook IDXGISwapChain::Present
	if (!g_original_present) {
		auto vtable = *(void***)swapchain->get_native();
		ensure(MH_CreateHook(vtable[8], &detour_present, (void**)&g_original_present), == MH_OK);
		ensure(MH_EnableHook(vtable[8]), == MH_OK);
	}

	// Create our window.
	if (!g_hwnd) {
		g_instance = GetModuleHandleW(nullptr);

		// Register window class.
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.lpfnWndProc = DefWindowProcW;
		wc.hInstance = g_instance;
		wc.lpszClassName = L"AU";
		g_window_class = RegisterClassExW(&wc);
		assert(g_window_class);

		// Create the window.
		constexpr DWORD style = WS_POPUP | WS_DISABLED;
		g_hwnd = CreateWindowExW(0, L"AU", L"AnyUpscale", style, 0, 0, g_window_width, g_window_height, (HWND)swapchain->get_hwnd(), nullptr, g_instance, nullptr);
		assert(g_hwnd);
		
		ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
	}

	// Create our swapchain.
	if (!g_swapchain) {
		// Get device and imidiate context.
		ensure(native_swapchain->GetDevice(IID_PPV_ARGS(g_device.reset_and_get_address())), >= 0);
		g_device->GetImmediateContext(g_ctx.reset_and_get_address());

		Com_ptr<IDXGIFactory2> factory2;
		ensure(native_swapchain->GetParent(IID_PPV_ARGS(&factory2)), >= 0);

		// Create the swapchain.
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.SampleDesc.Count = 1;
		swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
		swapchain_desc.BufferCount = 2;
		swapchain_desc.Scaling = DXGI_SCALING_NONE;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		ensure(factory2->CreateSwapChainForHwnd(g_device.get(), g_hwnd, &swapchain_desc, nullptr, nullptr, &g_swapchain), >= 0);
	}

	// Create our permanent resources.
	if (!g_smp_linear) {
		D3D11_SAMPLER_DESC sampler_desc = {};
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
		sampler_desc.MaxAnisotropy = 1;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		ensure(g_device->CreateSamplerState(&sampler_desc, &g_smp_linear), >= 0);
	}
	if (!g_vs_fullscreen_triangle) {
		create_vertex_shader(&g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
	}
	if (!g_rasterizer) {
		const CD3D11_RASTERIZER_DESC rasterizer_desc(D3D11_DEFAULT);
		g_device->CreateRasterizerState(&rasterizer_desc, &g_rasterizer);
	}
	if (!g_srv_src_back_buffer) {
		Com_ptr<ID3D11Texture2D> tex;
		ensure(native_swapchain->GetBuffer(0, IID_PPV_ARGS(&tex)), >= 0);
		ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_src_back_buffer), >= 0);
	}

	// Create other resources.
	create_resources(true);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "SourceWidth", g_src_width)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "SourceWidth", g_src_width);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "SourceHeight", g_src_height)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "SourceHeight", g_src_height);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "Upscaler", g_upscaler)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "Upscaler", g_upscaler);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "SGSRTarget", g_sgsr_target)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "SGSRTarget", g_sgsr_target);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "KernelBlur", g_power_of_garamond_blur)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "KernelBlur", g_power_of_garamond_blur);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "KernelSupport", g_power_of_garamond_support)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "KernelSupport", g_power_of_garamond_support);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "n", g_power_of_garamond_n)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "n", g_power_of_garamond_n);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "m", g_power_of_garamond_m)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "m", g_power_of_garamond_m);
	}
	if (!reshade::get_config_value(nullptr, "AnyUpscale", "Sharpness", g_amd_ffx_rcas_sharpness)) {
		reshade::set_config_value(nullptr, "AnyUpscale", "Sharpness", g_amd_ffx_rcas_sharpness);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	// Source dimensions.
	ImGui::InputInt("Source width", &g_src_width, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		reshade::set_config_value(nullptr, "AnyUpscale", "SourceWidth", g_src_width);
		create_resources(true);
	}
	ImGui::InputInt("Source height", &g_src_height, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		reshade::set_config_value(nullptr, "AnyUpscale", "SourceHeight", g_src_height);
		create_resources(true);
	}
	ImGui::Spacing();

	// Must be in the same order as enum UPSCALER.
	static constexpr std::array upscaler_items = { "SGSR1", "AMD FSR1 EASU", "Catmull-Rom", "Power of Garamond" };
	
	if (ImGui::Combo("Upscaler", &g_upscaler, upscaler_items.data(), upscaler_items.size())) {
		reshade::set_config_value(nullptr, "AnyUpscale", "Upscaler", g_upscaler);
		create_resources(true);
	}

	if (g_upscaler == UPSCALER_SGSR1) {
		// Must be in the same order as enum SGSR_TARGET.
		static constexpr std::array sgsr_target_items = { "High quality", "Mobile" };

		if (ImGui::Combo("Target", &g_sgsr_target, sgsr_target_items.data(), sgsr_target_items.size())) {
			reshade::set_config_value(nullptr, "AnyUpscale", "SGSRTarget", g_upscaler);
			g_ps_sgsr1.reset();
			create_resources();
		}
	}

	// Power of Garamond options.
	else if (g_upscaler == UPSCALER_POWER_OF_GARAMOND) {
		if (ImGui::SliderFloat("Blur", &g_power_of_garamond_blur, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
			reshade::set_config_value(nullptr, "AnyUpscale", "KernelBlur", g_power_of_garamond_blur);
			g_ps_power_of_garamond_x.reset();
			g_ps_power_of_garamond_y.reset();
			create_resources();
		}
		if (ImGui::SliderFloat("Support", &g_power_of_garamond_support, 1.0f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
			reshade::set_config_value(nullptr, "AnyUpscale", "KernelSupport", g_power_of_garamond_support);
			g_ps_power_of_garamond_x.reset();
			g_ps_power_of_garamond_y.reset();
			create_resources();
		}
		if (ImGui::SliderFloat("n", &g_power_of_garamond_n, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
			reshade::set_config_value(nullptr, "AnyUpscale", "n", g_power_of_garamond_n);
			g_ps_power_of_garamond_x.reset();
			g_ps_power_of_garamond_y.reset();
			create_resources();
		}
		if (ImGui::SliderFloat("m", &g_power_of_garamond_m, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
			reshade::set_config_value(nullptr, "AnyUpscale", "m", g_power_of_garamond_m);
			g_ps_power_of_garamond_x.reset();
			g_ps_power_of_garamond_y.reset();
			create_resources();
		}
	}

	// AMD FFX RCAS options.
	if (g_upscaler != UPSCALER_SGSR1) {
		ImGui::Spacing();
		if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_rcas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
			reshade::set_config_value(nullptr, "AnyUpscale", "Sharpness", g_amd_ffx_rcas_sharpness);
			g_ps_amd_ffx_rcas.reset();
			create_resources();
		}
	}
}

extern "C" __declspec(dllexport) const char* NAME = "AnyUpscale";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "AnyUpscale v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/AnyUpscale";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}
			read_config();
			g_any_upscale_path = get_any_upscale_path();
			set_dims();
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			DestroyWindow(g_hwnd);
			UnregisterClassW((LPCWSTR)g_window_class, g_instance);
			MH_Uninitialize();
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
