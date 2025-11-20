#include "Common.h"
#include "Helpers.h"
#include "AreaTex.h"
#include "SearchTex.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0

// We need to pass this to all SMAA shaders on resolution change.
class SMAA_rt_metrics
{
public:

	void set(float width, float height)
	{
		val_str = std::format("float4({},{},{},{})", 1.0f / width, 1.0f / height, width, height);
		defines[0] = { "SMAA_RT_METRICS", val_str.c_str() };
		defines[1] = { nullptr, nullptr };
	}

	const D3D_SHADER_MACRO* get() const
	{
		return defines;
	}

private:

	std::string val_str;
	D3D_SHADER_MACRO defines[2];
};

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

// FXAA
constexpr uint32_t g_ps_0x567D7B85_hash = 0x567D7B85;
constexpr GUID g_ps_0x567D7B85_guid = { 0xe3a0a9e2, 0x1e24, 0x47bf, { 0x95, 0xa, 0xa6, 0xa2, 0x27, 0xcb, 0xd2, 0xfa } };

static Com_ptr<ID3D11VertexShader> g_vs_fullscreen_triangle;
static Com_ptr<ID3D11PixelShader> g_ps_load;
static Com_ptr<ID3D11SamplerState> g_smp_point;

// SMAA
constexpr float g_smaa_clear_color[4] = {};
static SMAA_rt_metrics g_smaa_rt_metrics;
static Com_ptr<ID3D11ShaderResourceView> g_srv_smaa_area_tex;
static Com_ptr<ID3D11ShaderResourceView> g_srv_smaa_search_tex;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_edge_detection;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_edge_detection;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_blending_weight_calculation;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_blending_weight_calculation;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_neighborhood_blending;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_neighborhood_blending;
static Com_ptr<ID3D11DepthStencilState> g_ds_smaa_disable_depth_replace_stencil;
static Com_ptr<ID3D11DepthStencilState> g_ds_smaa_disable_depth_use_stencil;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

static std::filesystem::path get_shaders_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	return path;
}

static void compile_shader(ID3DBlob** code, const wchar_t* file, const char* target, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= file;
	Com_ptr<ID3DBlob> error;
	auto hr = D3DCompileFromFile(path.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code, &error);
	if (FAILED(hr)) {
		if (error) {
			log_error("D3DCompileFromFile: {}", (const char*)error->GetBufferPointer());
		}
		else {
			log_error("D3DCompileFromFile: (HRESULT){:08X}", hr);
		}
	}

	#if DEV && OUTPUT_ASSEMBLY
	Com_ptr<ID3DBlob> disassembly;
	D3DDisassemble((*code)->GetBufferPointer(), (*code)->GetBufferSize(), 0, nullptr, &disassembly);
	std::ofstream assembly(path.replace_filename(path.filename().string() + "_" + entry_point + ".asm"));
	assembly.write((const char*)disassembly->GetBufferPointer(), disassembly->GetBufferSize());
	#endif
}

static void create_vertex_shader(ID3D11Device* device, ID3D11VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "vs_5_0", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vs), >= 0);
}

static void create_pixel_shader(ID3D11Device* device, ID3D11PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "ps_5_0", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, ps), >= 0);
}

static void create_sampler_point(ID3D11Device* device, ID3D11SamplerState** smp)
{
	CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	static std::chrono::high_resolution_clock::time_point start;

	// We need to account for the acctual frame time.
	const auto sleep_time = g_frame_interval - std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start);

	// Precise sleep.
	const auto sleep_start = std::chrono::high_resolution_clock::now();
	std::this_thread::sleep_for(sleep_time - g_accounted_error);
	while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - sleep_start) < sleep_time) {
		continue;
	}

	start = std::chrono::high_resolution_clock::now();
}

static bool on_draw_indexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(&ps, nullptr, nullptr);
	if (!ps) {
		return false;
	}

	// 0x567D7B85 FXAA
	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_0x567D7B85_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x567D7B85_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(&device);

		// We expect RTV to be the backbuffer.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D11Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// We expect SRV in slot 0 to be the main scene (linear color).
		Com_ptr<ID3D11ShaderResourceView> srv_original;
		ctx->PSGetShaderResources(0, 1, &srv_original);

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(0, 1, &smp);
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Load (delinearize)
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_load) {
			create_pixel_shader(device.get(), &g_ps_load, L"Load_ps.hlsl");
		}

		// Create RT and views.
		//
		// The game laready renders FXAA directly to backbuffer with _srgb view, from rgba16f SRV.
		// Using higher bitness here probably wouldn't matter.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		Com_ptr<ID3D11RenderTargetView> rtv_delinearize;
		ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &rtv_delinearize), >= 0);
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		Com_ptr<ID3D11ShaderResourceView> srv_delinearize;
		ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &srv_delinearize), >= 0);

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_delinearize, nullptr);
		ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_load.get(), nullptr, 0);

		ctx->Draw(3, 0);

		//

		// SMAAEdgeDetection pass
		//

		// Create point sampler.
		[[unlikely]] if (!g_smp_point) {
			create_sampler_point(device.get(), &g_smp_point);
		}

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_edge_detection) {
			g_smaa_rt_metrics.set(tex_desc.Width, tex_desc.Height);
			create_vertex_shader(device.get(), &g_vs_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_edge_detection) {
			create_pixel_shader(device.get(), &g_ps_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_replace_stencil) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, &g_ds_smaa_disable_depth_replace_stencil), >= 0);
		}

		// Create DSV.
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11DepthStencilView> dsv;
		ensure(device->CreateDepthStencilView(tex.get(), nullptr, &dsv), >= 0);

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_edge_detection;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_smaa_edge_detection), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_edge_detection;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_smaa_edge_detection), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds_smaa_disable_depth_replace_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_edge_detection, dsv.get());
		ctx->VSSetShader(g_vs_smaa_edge_detection.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_edge_detection.get(), nullptr, 0);
		Com_ptr<ID3D11SamplerState> smp_original1;
		ctx->PSGetSamplers(1, 1, &smp_original1);
		ctx->PSSetSamplers(1, 1, &g_smp_point);
		ctx->PSSetShaderResources(0, 1, &srv_delinearize);

		ctx->ClearRenderTargetView(rtv_smaa_edge_detection.get(), g_smaa_clear_color);
		ctx->ClearDepthStencilView(dsv.get(), D3D10_CLEAR_STENCIL, 1.0f, 0);
		ctx->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device.get(), &g_vs_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device.get(), &g_ps_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!g_srv_smaa_area_tex) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = AREATEX_WIDTH;
			tex_desc.Height = AREATEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = areaTexBytes;
			subresource_data.SysMemPitch = AREATEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.reset_and_get_address()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_smaa_area_tex), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!g_srv_smaa_search_tex) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = SEARCHTEX_WIDTH;
			tex_desc.Height = SEARCHTEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = searchTexBytes;
			subresource_data.SysMemPitch = SEARCHTEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.reset_and_get_address()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_smaa_search_tex), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_use_stencil) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT); 
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, &g_ds_smaa_disable_depth_use_stencil), >= 0);
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_blending_weight_calculation;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_smaa_blending_weight_calculation), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_blending_weight_calculation;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_smaa_blending_weight_calculation), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds_smaa_disable_depth_use_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_blending_weight_calculation, dsv.get());
		ctx->VSSetShader(g_vs_smaa_blending_weight_calculation.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_blending_weight_calculation.get(), nullptr, 0);
		const std::array ps_srvs_smaa_blending_weight_calculation = { srv_smaa_edge_detection.get(), g_srv_smaa_area_tex.get(), g_srv_smaa_search_tex.get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_blending_weight_calculation.size(), ps_srvs_smaa_blending_weight_calculation.data());

		ctx->ClearRenderTargetView(rtv_smaa_blending_weight_calculation.get(), g_smaa_clear_color);
		ctx->Draw(3, 0);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_neighborhood_blending) {
			create_vertex_shader(device.get(), &g_vs_smaa_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_neighborhood_blending) {
			create_pixel_shader(device.get(), &g_ps_smaa_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->VSSetShader(g_vs_smaa_neighborhood_blending.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_neighborhood_blending.get(), nullptr, 0);
		const std::array ps_srvs_neighborhood_blending = { srv_original.get(), srv_smaa_blending_weight_calculation.get() };
		ctx->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

		ctx->Draw(3, 0);

		//

		// Restore original states.
		ctx->PSSetSamplers(1, 1, &smp_original1);

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
			switch (hash) {
				case g_ps_0x567D7B85_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x567D7B85_guid, sizeof(g_ps_0x567D7B85_hash), &g_ps_0x567D7B85_hash), >= 0);
					break;
			}
		}
	}
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_refresh_rate = 0.0f;
	desc.sync_interval = 0;

	// Reset resolution dependent resources.
	g_vs_smaa_edge_detection.reset();
	g_ps_smaa_edge_detection.reset();
	g_vs_smaa_blending_weight_calculation.reset();
	g_ps_smaa_blending_weight_calculation.reset();
	g_vs_smaa_neighborhood_blending.reset();
	g_ps_smaa_neighborhood_blending.reset();

	return true;
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(&device1);
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "FrostpunkGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "FrostpunkGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "FrostpunkGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "FrostpunkGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 10.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "FrostpunkGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "FrostpunkGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "FrostpunkGrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "FrostpunkGrapicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/FrostpunkGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
