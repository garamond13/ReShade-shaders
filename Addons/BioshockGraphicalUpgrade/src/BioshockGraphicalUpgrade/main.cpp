#include "Common.h"
#include "Helpers.h"
#include "AreaTex.h"
#include "SearchTex.h"
#include "BlueNoiseTex.h"

#define DEV 0

// Constant buffer
//

struct alignas(16) Constant_buffer_data
{
	float tex_noise_index = 0.0f;
};

static Constant_buffer_data g_cb_data;
static Com_ptr<ID3D10Buffer> g_cb;

//

// Tone responce curve
//

enum TRC
{
	TRC_SRGB,
	TRC_GAMMA
};

// TRC
static int g_trc = TRC_GAMMA;

static float g_gamma = 2.2f;

//

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

static Com_ptr<ID3D10VertexShader> g_vs_fullscreen_triangle;

// Tone maping
// The last shader before UI.
constexpr uint32_t g_ps_0x87A0B43D_hash = 0x87A0B43D;
constexpr GUID g_ps_0x87A0B43D_guid = { 0x476ef3b9, 0xe14e, 0x49f2, { 0xa6, 0x5c, 0x8c, 0x7a, 0x41, 0xd4, 0xa6, 0x12 } };
static Com_ptr<ID3D10PixelShader> g_ps_0x87A0B43D;

static Com_ptr<ID3D10PixelShader> g_ps_delinearize;

// SMAA
constexpr float g_smaa_clear_color[4] = {};
static SMAA_rt_metrics g_smaa_rt_metrics;
static Com_ptr<ID3D10ShaderResourceView> g_srv_area_tex;
static Com_ptr<ID3D10ShaderResourceView> g_srv_search_tex;
static Com_ptr<ID3D10VertexShader> g_vs_smaa_edge_detection;
static Com_ptr<ID3D10PixelShader> g_ps_smaa_edge_detection;
static Com_ptr<ID3D10VertexShader> g_vs_smaa_blending_weight_calculation;
static Com_ptr<ID3D10PixelShader> g_ps_smaa_blending_weight_calculation;
static Com_ptr<ID3D10VertexShader> g_vs_neighborhood_blending;
static Com_ptr<ID3D10PixelShader> g_ps_neighborhood_blending;

// AMD FFX CAS
static Com_ptr<ID3D10PixelShader> g_ps_amd_ffx_cas;
static float g_amd_ffx_cas_sharpness = 0.4f;

// AMD FFX LFGA
static Com_ptr<ID3D10ShaderResourceView> g_srv_blue_noise_tex;
static Com_ptr<ID3D10PixelShader> g_ps_amd_ffx_lfga;
static float g_amd_ffx_lfga_amount = 0.4f;

// FPS Limiter
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

// Internal only.
static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

static float g_bloom_intensity = 1.0f;

// ReShade API can't properly handle some things that we need.
// Examples:
// - Rebinding back buffer.
// - DXGI_PRESENT_ALLOW_TEARING flag, even it should.
// - FPS limiter gets halved unless we do some hacks.
// TODO: Recheck can we just use ReShade API instead of our own hook.
typedef HRESULT(__stdcall *IDXGISwapChain__Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static IDXGISwapChain__Present g_original_present;

static std::filesystem::path get_shaders_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	return path;
}

static void read_config()
{
	// Get/set TRC.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
	}

	// Get/set gamma.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
	}

	// Get/set bloom intensity.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}

	// Get/set sharpness.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}

	// Get/set grain amount.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
	}

	// Get/set FPS limit.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);

	// Get/set accounted error.
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void create_vertex_shader(ID3D10Device* device, ID3D10VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D10_SHADER_MACRO* shader_macros = nullptr)
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= file;
	Com_ptr<ID3D10Blob> code;
	Com_ptr<ID3D10Blob> error;
	auto hr = D3DCompileFromFile(path.c_str(), shader_macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, "vs_4_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
	if (FAILED(hr)) {
		if (error) {
			MessageBoxA(nullptr, (LPCSTR)error->GetBufferPointer(), "ERROR: D3DCompileFromFile", MB_OK);
		}
		else {
			const auto text = "HRESULT: " + std::format("{:08X}", hr);
			MessageBoxA(nullptr, text.c_str(), "ERROR: D3DCompileFromFile", MB_OK);
		}
	}
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), vs), >= 0);
}

static void create_pixel_shader(ID3D10Device* device, ID3D10PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D10_SHADER_MACRO* shader_macros = nullptr)
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= file;
	Com_ptr<ID3D10Blob> code;
	Com_ptr<ID3D10Blob> error;
	auto hr = D3DCompileFromFile(path.c_str(), shader_macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, "ps_4_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &error);
	if (FAILED(hr)) {
		if (error) {
			MessageBoxA(nullptr, (LPCSTR)error->GetBufferPointer(), "ERROR: D3DCompileFromFile", MB_OK);
		}
		else {
			const auto text = "HRESULT: " + std::format("{:08X}", hr);
			MessageBoxA(nullptr, text.c_str(), "ERROR: D3DCompileFromFile", MB_OK);
		}
	}
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), ps), >= 0);
}

static void create_constant_buffer(ID3D10Device* device, ID3D10Buffer** buffer, UINT size)
{
	D3D10_BUFFER_DESC buffer_desc = {};
	buffer_desc.ByteWidth = size;
	buffer_desc.Usage = D3D10_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	ensure(device->CreateBuffer(&buffer_desc, nullptr, buffer), >= 0);
}

static void update_constant_buffer(ID3D10Buffer* buffer, void* data, size_t size)
{
	void* mapped;
	ensure(buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped), >= 0);
	std::memcpy(mapped, data, size);
	buffer->Unmap();
}

// Used by SMAA.
static void create_srv_area_tex(ID3D10Device* device, ID3D10ShaderResourceView** srv)
{
	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = AREATEX_WIDTH;
	tex_desc.Height = AREATEX_HEIGHT;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	D3D10_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pSysMem = areaTexBytes;
	subresource_data.SysMemPitch = AREATEX_PITCH;
	Com_ptr<ID3D10Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, &subresource_data, &tex), >= 0);
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv), >= 0);
}

// Used by SMAA.
static void create_srv_search_tex(ID3D10Device* device, ID3D10ShaderResourceView** srv)
{
	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = SEARCHTEX_WIDTH;
	tex_desc.Height = SEARCHTEX_HEIGHT;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8_UNORM;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	D3D10_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pSysMem = searchTexBytes;
	subresource_data.SysMemPitch = SEARCHTEX_PITCH;
	Com_ptr<ID3D10Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, &subresource_data, &tex), >= 0);
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv), >= 0);
}

// Used by AMD FFX LFGA.
static void create_srv_blue_noise_tex(ID3D10Device* device, ID3D10ShaderResourceView** srv)
{
	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = BLUE_NOISE_TEX_WIDTH;
	tex_desc.Height = BLUE_NOISE_TEX_HEIGHT;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = BLUE_NOISE_TEX_ARRAY_SIZE;
	tex_desc.Format = DXGI_FORMAT_R8_UNORM;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	std::array<D3D10_SUBRESOURCE_DATA, BLUE_NOISE_TEX_ARRAY_SIZE> subresource_data;
	for (size_t i = 0; i < subresource_data.size(); ++i) {
		subresource_data[i].pSysMem = BLUE_NOISE_TEX + i * BLUE_NOISE_TEX_PITCH * BLUE_NOISE_TEX_HEIGHT;
		subresource_data[i].SysMemPitch = BLUE_NOISE_TEX_PITCH;
	}
	Com_ptr<ID3D10Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, subresource_data.data(), &tex), >= 0);
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv), >= 0);
}

// Used by AMD FFX LFGA.
static void create_smp_noise(ID3D10Device* device, ID3D10SamplerState** smp)
{
	D3D10_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MaxLOD = D3D10_FLOAT32_MAX;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D10_COMPARISON_NEVER;
	ensure(device->CreateSamplerState(&sampler_desc, smp), >= 0);
}

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	// We have to rebind back buffer and DS later for flip mode to work in this game.
	Com_ptr<ID3D10Device> device;
	ensure(swapchain->GetDevice(IID_PPV_ARGS(&device)), >= 0);
	Com_ptr<ID3D10RenderTargetView> rtv;
	Com_ptr<ID3D10DepthStencilView> dsv;
	device->OMGetRenderTargets(1, &rtv, &dsv);
	
	// Limit FPS
	//

	static std::chrono::high_resolution_clock::time_point start;

	// We need to account for the acctual frame time.
	const auto sleep_time = g_frame_interval - std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start);

	// Precise sleep
	////

	const auto sleep_start = std::chrono::high_resolution_clock::now();

	// Looks like we don't need to set timeBeginPeriod(1) for the best accuracy. 
	std::this_thread::sleep_for(sleep_time - g_accounted_error);

	while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - sleep_start) < sleep_time) {
		continue;
	}

	////

	start = std::chrono::high_resolution_clock::now();
	
	//
	
	flags |= DXGI_PRESENT_ALLOW_TEARING;
	auto hr = g_original_present(swapchain, sync_interval, flags);
	
	// Rebind back buffer and DS.
	device->OMSetRenderTargets(1, &rtv, dsv.get());
	
	return hr;
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	// We can't reliably track shaders that we need via handle alone,
	// so we will use private data.
	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(&ps);
	if (!ps) {
		return false;
	}
	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_0x87A0B43D_guid, &size, &hash);

	if (SUCCEEDED(hr) && hash == g_ps_0x87A0B43D_hash) {

		// We expect RTV to be a back buffer.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// Maybe we should skip all this and just make some assumptions?
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource.as(tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// 0x87A0B43D pass
		//

		// Create and bind PS.
		[[unlikely]] if (!g_ps_0x87A0B43D) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D10_SHADER_MACRO shader_macros[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_0x87A0B43D, L"0x87A0B43D_ps.hlsl", "main", shader_macros);
		}
		device->PSSetShader(g_ps_0x87A0B43D.get());

		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		RT_views rt_views_0x87A0B43D(device, tex_desc);

		// Render 0x87A0B43D pass.
		device->OMSetRenderTargets(1, rt_views_0x87A0B43D.get_rtv_address(), nullptr);
		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		#if DEV
		// Primitive topology should be D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology;
		device->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(0, 1, &smp);
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		device->PSGetSamplers(1, 1, smp.reset_and_get_address());
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Delinearize pass
		//

		// Create and bind Fullscreen Triangle VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}
		device->VSSetShader(g_vs_fullscreen_triangle.get());

		// Create and bind PS.
		[[unlikely]] if (!g_ps_delinearize) {
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO shader_macros[] = {
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_delinearize, L"Delinearize_ps.hlsl", "main", shader_macros);
		}
		device->PSSetShaderResources(0, 1, rt_views_0x87A0B43D.get_srv_address());
		device->PSSetShader(g_ps_delinearize.get());

		RT_views rt_views_delinearize(device, tex_desc);

		// Render Delinearize pass.
		device->OMSetRenderTargets(1, rt_views_delinearize.get_rtv_address(), nullptr);
		device->Draw(3, 0);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		// TODO: Optimize SMAA performance with depth stencil.
		
		// SMAAEdgeDetection pass
		//
		// In my tests using depth as predication texture performs slightly worse in terms of quality.
		// Performance gains shouldn't be net positive since we have to linearize depth in the seperate pass.
		//

		// Create and bind VS.
		[[unlikely]] if (!g_vs_smaa_edge_detection) {
			create_vertex_shader(device, &g_vs_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get(tex_desc.Width, tex_desc.Height));
		}
		device->VSSetShader(g_vs_smaa_edge_detection.get());

		// Create and bind PS.
		[[unlikely]] if (!g_ps_smaa_edge_detection) {
			create_pixel_shader(device, &g_ps_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}
		device->PSSetShaderResources(0, 1, rt_views_delinearize.get_srv_address());
		device->PSSetShader(g_ps_smaa_edge_detection.get());

		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		RT_views rt_views_edges_tex(device, tex_desc);

		// Render SMAAEdgeDetection pass.
		device->ClearRenderTargetView(rt_views_edges_tex.get_rtv(), g_smaa_clear_color);
		device->OMSetRenderTargets(1, rt_views_edges_tex.get_rtv_address(), nullptr);
		device->Draw(3, 0);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create and bind VS.
		[[unlikely]] if (!g_vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device, &g_vs_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}
		device->VSSetShader(g_vs_smaa_blending_weight_calculation.get());

		// Create and bind PS.
		[[unlikely]] if (!g_srv_area_tex) {
			create_srv_area_tex(device, &g_srv_area_tex);
		}
		[[unlikely]] if (!g_srv_search_tex) {
			create_srv_search_tex(device, &g_srv_search_tex);
		}
		[[unlikely]] if (!g_ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device, &g_ps_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}
		const std::array ps_resources_smaa_blending_weight_calculation = { rt_views_edges_tex.get_srv(), g_srv_area_tex.get(), g_srv_search_tex.get() };
		device->PSSetShaderResources(0, ps_resources_smaa_blending_weight_calculation.size(), ps_resources_smaa_blending_weight_calculation.data());
		device->PSSetShader(g_ps_smaa_blending_weight_calculation.get());

		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		RT_views rt_views_blend_tex(device, tex_desc);

		// Render SMAABlendingWeightCalculation pass.
		device->ClearRenderTargetView(rt_views_blend_tex.get_rtv(), g_smaa_clear_color);
		device->OMSetRenderTargets(1, rt_views_blend_tex.get_rtv_address(), nullptr);
		device->Draw(3, 0);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create and bind VS.
		[[unlikely]] if (!g_vs_neighborhood_blending) {
			create_vertex_shader(device, &g_vs_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}
		device->VSSetShader(g_vs_neighborhood_blending.get());

		// Create and bind PS.
		[[unlikely]] if (!g_ps_neighborhood_blending) {
			create_pixel_shader(device, &g_ps_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}
		const std::array ps_resources_neighborhood_blending = { rt_views_0x87A0B43D.get_srv(), rt_views_blend_tex.get_srv() };
		device->PSSetShaderResources(0, ps_resources_neighborhood_blending.size(), ps_resources_neighborhood_blending.data());
		device->PSSetShader(g_ps_neighborhood_blending.get());

		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		RT_views rt_views_resources_neighborhood_blending(device, tex_desc);

		// Render SMAANeighborhoodBlending pass.
		device->OMSetRenderTargets(1, rt_views_resources_neighborhood_blending.get_rtv_address(), nullptr);
		device->Draw(3, 0);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		// AMD FFX CAS pass
		//

		// Create and bind VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}
		device->VSSetShader(g_vs_fullscreen_triangle.get());

		// Create and bind PS.
		[[unlikely]] if (!g_ps_amd_ffx_cas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D10_SHADER_MACRO shader_macros[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_amd_ffx_cas, L"AMD_FFX_CAS_ps.hlsl", "main", shader_macros);
		}
		device->PSSetShaderResources(0, 1, rt_views_resources_neighborhood_blending.get_srv_address());
		device->PSSetShader(g_ps_amd_ffx_cas.get());

		RT_views rt_views_amd_ffx_cas(device, tex_desc);

		// Render AMD FFX CAS pass.
		device->OMSetRenderTargets(1, rt_views_amd_ffx_cas.get_rtv_address(), nullptr);
		device->Draw(3, 0);
		device->OMSetRenderTargets(0, nullptr, nullptr);

		//

		// AMD FFX LFGA pass
		//
		
		// Create and bind PS.
		////

		[[unlikely]] if (!g_cb) {
			create_constant_buffer(device, &g_cb, sizeof(g_cb_data));
		}
		[[unlikely]] if (!g_srv_blue_noise_tex) {
			create_srv_blue_noise_tex(device, &g_srv_blue_noise_tex);
		}
		static Com_ptr<ID3D10SamplerState> g_smp_noise;
		[[unlikely]] if (!g_smp_noise) {
			create_smp_noise(device, &g_smp_noise);
		}
		[[unlikely]] if (!g_ps_amd_ffx_lfga) {
			const std::string viewport_dims = std::format("float2({},{})", (float)tex_desc.Width, (float)tex_desc.Height);
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO shader_macros[] = {
				{ "WIEWPORT_DIMS", viewport_dims.c_str() },
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_amd_ffx_lfga, L"AMD_FFX_LFGA_ps.hlsl", "main", shader_macros);
		}

		// Update the constant buffer.
		// We need to limit, otherwise grain will flicker.
		constexpr auto interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / (double)BLUE_NOISE_TEX_ARRAY_SIZE));
		static auto last_update = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		if (now - last_update >= interval) {
			g_cb_data.tex_noise_index += 1.0f;
			if (g_cb_data.tex_noise_index >= (float)BLUE_NOISE_TEX_ARRAY_SIZE) {
				g_cb_data.tex_noise_index = 0.0f;
			}
			update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		device->PSSetConstantBuffers(13, 1, &g_cb); // The game should never be using CB slot 13.
		const std::array ps_resources_amd_ffx_lfga = { rt_views_amd_ffx_cas.get_srv(), g_srv_blue_noise_tex.get() };
		device->PSSetShaderResources(0, ps_resources_amd_ffx_lfga.size(), ps_resources_amd_ffx_lfga.data());
		Com_ptr<ID3D10SamplerState> smp_original0;
		device->PSGetSamplers(0, 1, &smp_original0);
		device->PSSetSamplers(0, 1, &g_smp_noise);
		device->PSSetShader(g_ps_amd_ffx_lfga.get());

		////

		// Render AMD FFX LFGA pass.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->Draw(3, 0);
		
		//

		// Restore original states.
		device->PSSetSamplers(0, 1, &smp_original0);

		return true;
	}

	return false;
}

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			if (hash == g_ps_0x87A0B43D_hash) {
				ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x87A0B43D_guid, sizeof(g_ps_0x87A0B43D_hash), &g_ps_0x87A0B43D_hash), >= 0);
			}
		}
	}
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Upgrade render targets.
	if (((desc.usage & reshade::api::resource_usage::render_target) != 0)) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	// Try to filter only render targets that we have upgraded.
	const auto resource_desc = device->get_resource_desc(resource);
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
			desc.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	// The game already uses anistropyc filtering where it matters.
	desc.max_anisotropy = 16.0f;

	return true;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// Set the window to borderless fullscreen, the main intention.
	// Non fullscreen window with or without border can be still set from the game if first set as windowed mode.
	// Fixes UI (menus and loading screen) being pushed below lower screen border.
	// Fixes ReShade overlay being smudged/blured.
	SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
	SetWindowPos((HWND)hwnd, HWND_TOP, 0, 0, desc.back_buffer.texture.width, desc.back_buffer.texture.height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_state = false;
	desc.fullscreen_refresh_rate = 0.0;
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;

	// Reset resolution dependent resources.
	g_vs_smaa_edge_detection.reset();
	g_ps_smaa_edge_detection.reset();
	g_vs_smaa_blending_weight_calculation.reset();
	g_ps_smaa_blending_weight_calculation.reset();
	g_vs_neighborhood_blending.reset();
	g_ps_neighborhood_blending.reset();
	g_smaa_rt_metrics.invalidate();
	g_ps_amd_ffx_lfga.reset();

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	// Hook IDXGISwapChain::Present
	static bool hook_installed = false;
	if (!hook_installed) {
		auto vtable = *(void***)swapchain->get_native();
		if (MH_CreateHook(vtable[8], &detour_present, (void**)&g_original_present) == MH_OK && MH_EnableHook(vtable[8]) == MH_OK) {
			hook_installed = true;
		}
	}
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	// It reduces input latecy massivly if GPU bound.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(IID_PPV_ARGS(&device1));
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	// Set TRC
	constexpr std::array g_trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, g_trc_items.data(), g_trc_items.size())) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
		g_ps_delinearize.reset();
		g_ps_amd_ffx_lfga.reset();
	}

	// Set gamma
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
		g_ps_delinearize.reset();
		g_ps_amd_ffx_lfga.reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	// Set bloom intensity.
	if (ImGui::SliderFloat("Bloom Intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps_0x87A0B43D.reset();
	}
	ImGui::Spacing();

	// Set sharpness.
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::Spacing();

	// Set grain amount.
	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps_amd_ffx_lfga.reset();
	}
	ImGui::Spacing();

	// Set frame interval.
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 10.0f, 1000.0f);
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	// Set accounted error.
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "BioshockGrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "BioshockGrapicalUpgrade v1.1.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BioshockGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			MH_Uninitialize();
			break;
	}
	return TRUE;
}
