#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "MiddleEarthShadowOfWarGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/MiddleEarthShadowOfWarGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_cs_linearize_depth_and_generate_mvs_0x0E095BB1 = { 0x0E095BB1, { 0x5a0bf1c1, 0x0, 0x4ceb, { 0xa6, 0x81, 0x38, 0xa6, 0x6e, 0x53, 0xfb, 0x1 }}}; 
constexpr Shader_hash g_ps_taa_0x8D06D556 = { 0x8D06D556, { 0x24291891, 0x2770, 0x4bd8, { 0xbd, 0xb9, 0xd7, 0x9a, 0xf, 0x7d, 0x1c, 0x5 }}};
constexpr Shader_hash g_ps_tonemap_0xFD32C367 = { 0xFD32C367, { 0xa2cffb88, 0x321, 0x461b, { 0x8e, 0x96, 0xfb, 0x84, 0xb0, 0x52, 0xfb, 0xa3 }}};
constexpr Shader_hash g_ps_blend_ui_0x09C22C0E = { 0x09C22C0E, { 0x8d968089, 0x4c32, 0x4f2e, { 0xa3, 0xcf, 0xcb, 0xec, 0x60, 0x4, 0x97, 0x62 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D11Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
static float g_amd_ffx_cas_sharpness = 0.0f;
static float g_amd_ffx_lfga_amount = 0.3f;

// DLSS
constexpr int g_dlss_flags{
	NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
	NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
	NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
};
static NVSDK_NGX_DLSS_Hint_Render_Preset g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
static int g_user_set_dlss_preset;
static bool g_enable_dlss;
static bool g_dlss_status;
static float g_jitter_x;
static float g_jitter_y;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(ps.put(), nullptr, nullptr);
	if (!ps) {
		return false;
	}

	#if DEV
	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_taa_0x8D06D556.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_taa_0x8D06D556.hash) {
		if (g_enable_dlss) {
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			std::array<ID3D11ShaderResourceView*, 2> srvs;
			ctx->PSGetShaderResources(0, srvs.size(), srvs.data());

			Com_ptr<ID3D11Resource> resource_mvs;
			srvs[0]->GetResource(resource_mvs.put());
			Com_ptr<ID3D11Resource> resource_scene;
			srvs[1]->GetResource(resource_scene.put());

			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			Com_ptr<ID3D11Resource> resource_rt;
			rtv->GetResource(resource_rt.put());

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = resource_scene.get();
			eval_params.Feature.pInOutput = resource_rt.get();
			eval_params.pInDepth = g_managed_resources.resources["depth"_h].get();
			eval_params.pInMotionVectors = resource_mvs.get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			eval_params.InMVScaleX = -g_swapchain_width;
			eval_params.InMVScaleY = -g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in range [-1, 1].
			eval_params.InJitterOffsetX = g_jitter_x * -0.5f;
			eval_params.InJitterOffsetY = g_jitter_y * 0.5f;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			release_com_array(srvs);

			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xFD32C367.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xFD32C367.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0xFD32C367"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0xFD32C367"_h].put(), L"Tonemap_0xFD32C367_ps.hlsl");
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["tonemap_0xFD32C367__amd_ffx_lfga"_h]) {
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			
			// Get texture description.
			Com_ptr<ID3D11Resource> resource;
			rtv->GetResource(resource.put());
			Com_ptr<ID3D11Texture2D> tex;
			resource->QueryInterface(tex.put());
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["tonemap_0xFD32C367__amd_ffx_lfga"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["tonemap_0xFD32C367__amd_ffx_lfga"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0xFD32C367__amd_ffx_lfga"_h], nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0xFD32C367"_h].get(), nullptr, 0);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		// AMD FFX CAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(g_device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_cas"_h]) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["amd_ffx_cas"_h].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["amd_ffx_cas"_h]) {
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			
			// Get texture description.
			Com_ptr<ID3D11Resource> resource;
			rtv->GetResource(resource.put());
			Com_ptr<ID3D11Texture2D> tex;
			resource->QueryInterface(tex.put());
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["amd_ffx_cas"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["amd_ffx_cas"_h], nullptr);
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0xFD32C367__amd_ffx_lfga"_h]);

		ctx->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_lfga"_h]) {
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const D3D_SHADER_MACRO defines[] = {
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
		}

		// Update the constant buffer.
		// We need to limit the temporal grain update rate, otherwise grain will flicker.
		constexpr double update_rate = 64.0;
		constexpr int pattern_lenght = 1024;
		constexpr auto interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / update_rate));
		static auto last_update = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		if (now - last_update >= interval) {
			g_cb_data.noise_index += 1;
			if (g_cb_data.noise_index >= pattern_lenght) {
				g_cb_data.noise_index = 0;
			}
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0xFD32C367__amd_ffx_lfga"_h], nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["amd_ffx_cas"_h]);

		ctx->Draw(3, 0);

		//

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_blend_ui_0x09C22C0E.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_blend_ui_0x09C22C0E.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["blend_ui_0x09C22C0E"_h]) {
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D_SHADER_MACRO defines[] = {
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["blend_ui_0x09C22C0E"_h].put(), L"BlendUI_0x09C22C0E_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["blend_ui_0x09C22C0E"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0xFD32C367__amd_ffx_lfga"_h]);

		return false;
	}

	return false;
}

static bool on_dispatch(reshade::api::command_list* cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11ComputeShader> cs;
	ctx->CSGetShader(cs.put(), nullptr, nullptr);

	#if DEV
	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.hash) {
		Com_ptr<ID3D11ShaderResourceView> srv;
		ctx->CSGetShaderResources(1, 1, srv.put());
		srv->GetResource(g_managed_resources.resources["depth"_h].put());
		return false;
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
				case g_ps_taa_0x8D06D556.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_taa_0x8D06D556.guid, sizeof(g_ps_taa_0x8D06D556.hash), &g_ps_taa_0x8D06D556.hash), >= 0);
					return;
				case g_ps_tonemap_0xFD32C367.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xFD32C367.guid, sizeof(g_ps_tonemap_0xFD32C367.hash), &g_ps_tonemap_0xFD32C367.hash), >= 0);
					return;
				case g_ps_blend_ui_0x09C22C0E.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_blend_ui_0x09C22C0E.guid, sizeof(g_ps_blend_ui_0x09C22C0E.hash), &g_ps_blend_ui_0x09C22C0E.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.guid, sizeof(g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.hash), &g_cs_linearize_depth_and_generate_mvs_0x0E095BB1.hash), >= 0);
					return;
			}
		}
	}
}

static bool on_update_buffer_region(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
{
	auto native_resource = (ID3D11Resource*)resource.handle;
	Com_ptr<ID3D11Buffer> buffer;
	auto hr = native_resource->QueryInterface(buffer.put());
	if (SUCCEEDED(hr)) {
		D3D11_BUFFER_DESC desc;
		buffer->GetDesc(&desc);

		// This alone should be reliable? Needs testing!
		if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 544) {
			// cb0[31].xy in PS TAA 0x8D06D556.
			g_jitter_x = ((float4*)data)[31].x;
			g_jitter_y = ((float4*)data)[31].y;
		}
	}
	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r32g32_float) {
			desc.format = reshade::api::format::r32g32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	// Upgrading r8g8b8a8_unorm is crashing the game.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// As recommended for DLAA.
		desc.mip_lod_bias += -1.0f;
	}

	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	#if 0
	return false;
	#endif

	// The game supports HDR which is untested.
	if (desc.back_buffer.texture.format == reshade::api::format::r8g8b8a8_unorm) {
		desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	}

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	// The game is creating 2 swapchains, but they should be on the same device and use the same description.

	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	native_swapchain->GetDesc(&desc);

	// Save device.
	ensure(native_swapchain->GetDevice(IID_PPV_ARGS(&g_device)), >= 0);

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Set maximum frame latency to 1.
	Com_ptr<IDXGIDevice1> dxgi_device1;
	auto hr = g_device->QueryInterface(dxgi_device1.put());
	if (SUCCEEDED(hr)) {
		ensure(dxgi_device1->SetMaximumFrameLatency(1), >= 0);
	}

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		if (!resize) {
			DLSS::get_instance().init(g_device);
		}
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
	}

	// Reset reolution dependent resources.
	g_managed_resources.render_target_views["tonemap_0xFD32C367__amd_ffx_lfga"_h].reset();
	g_managed_resources.render_target_views["amd_ffx_cas"_h].reset();
}

static void on_destroy_device(reshade::api::device* device)
{
	if (device->get_native() != (uintptr_t)g_device) {
		return;
	}
	if (g_enable_dlss) {
		DLSS::get_instance().shutdown();
	}
	g_cb.reset();
	g_managed_resources.clear();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss);
	}

	if (!reshade::get_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset)) {
		reshade::set_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset);
	}
	switch (g_user_set_dlss_preset) {
			case 0: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default; break;
			case 1: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_E; break;
			case 2: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F; break;
			case 3: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_K; break;
			case 4: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_L; break;
			case 5: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_M; break;
			default: assert(false);
	}

	if (!reshade::get_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount);
	}
	if (!reshade::get_config_value(nullptr, NAME, "TRC", g_trc)) {
		reshade::set_config_value(nullptr, NAME, "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, NAME, "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, NAME, "Gamma", g_gamma);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	#if DEV
	if (ImGui::Button("Dev button")) {
	}
	ImGui::Spacing();

	// The game may set this a bit later.
	if (ImGui::Button("Check MaximumFrameLatency")) {
		Com_ptr<IDXGIDevice1> dxgi_device;
		ensure(g_device->QueryInterface(dxgi_device.put()), >= 0);
		UINT max_latency;
		ensure(dxgi_device->GetMaximumFrameLatency(&max_latency), >= 0);
		log_debug("MaximumFrameLatency: {}", max_latency);
	}
	ImGui::NewLine();
	#endif

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			Com_ptr<ID3D11DeviceContext> ctx;
			g_device->GetImmediateContext(ctx.put());
			DLSS::get_instance().init(g_device);
			DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
		}
		else {
			DLSS::get_instance().shutdown();
		}
		reshade::set_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "Default", "E", "F", "K", "L", "M" };
	if (ImGui::Combo("DLSS preset", &g_user_set_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		switch (g_user_set_dlss_preset) {
			case 0: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default; break;
			case 1: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_E; break;
			case 2: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F; break;
			case 3: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_K; break;
			case 4: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_L; break;
			case 5: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_M; break;
			default: assert(false);
		}
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
		reshade::set_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset);
	}
	if (g_enable_dlss) {
		if (g_dlss_status) {
			ImGui::Text("DLSS status: OK.");
		}
		else {
			ImGui::Text("DLSS status: Faild or not running!");
		}
		g_dlss_status = false;
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["amd_ffx_cas"_h].reset();
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount);
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		g_managed_resources.pixel_shaders["blend_ui_0x09C22C0E"_h].reset();
		reshade::set_config_value(nullptr, NAME, "TRC", g_trc);
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["blend_ui_0x09C22C0E"_h].reset();
		reshade::set_config_value(nullptr, NAME, "Gamma", g_gamma);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			init_graphical_upgrade_path();
			read_config();
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::update_buffer_region>(on_update_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
