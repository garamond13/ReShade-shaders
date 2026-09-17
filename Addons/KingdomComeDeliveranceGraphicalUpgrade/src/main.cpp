#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "KingdomComeDeliveranceGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/KingdomComeDeliveranceGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_taa_0x505343B8 = { 0x505343B8, { 0x3c9eedcf, 0x3f2c, 0x43a0, { 0xbd, 0x43, 0xe5, 0xae, 0x59, 0xad, 0xd4, 0xad }}};
constexpr Shader_hash g_cs_lightning_0x505343B8 = { 0x0181192D, { 0x10035773, 0x5788, 0x4bdb, { 0x97, 0x28, 0xa5, 0x76, 0x34, 0x78, 0xfe, 0xf6 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static bool g_has_drawn_lightning;
static std::unordered_map<uintptr_t, void*> g_mapped_cbs;

// DLSS
constexpr int g_dlss_flags{
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

// MSVC still has no `constexpr std::round`!
static constexpr int constexpr_round(float x)
{
	return x < 0.0f ? (int)(x - 0.5f) : (int)(x + 0.5f);
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	g_has_drawn_lightning = false;
	g_mapped_cbs.clear();

	// We have to rebind RTV and DSV after present for `DXGI_SWAP_EFFECT_FLIP_DISCARD` to work properly in videos and loading screens.
	Com_ptr<ID3D11DeviceContext> ctx;
	g_device->GetImmediateContext(ctx.put());
	ctx->OMGetRenderTargets(1, g_managed_resources.render_target_views["on_present"_h].put(), g_managed_resources.depth_stencil_views["on_present"_h].put());
}

static void on_finish_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain)
{
	Com_ptr<ID3D11DeviceContext> ctx;
	g_device->GetImmediateContext(ctx.put());
	ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["on_present"_h], g_managed_resources.depth_stencil_views["on_present"_h].get());
}

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
	hr = ps->GetPrivateData(g_ps_taa_0x505343B8.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_taa_0x505343B8.hash) {
		if (g_enable_dlss) {
			// DLSS requires an immediate context for execution!
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// Get RTV and it's resource.
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			Com_ptr<ID3D11Resource> resource_output;
			rtv->GetResource(resource_output.put());

			// MVs pass
			//

			// Create PS.
			[[unlikely]] if (!g_managed_resources.pixel_shaders["taa_0x505343B8"_h]) {
				create_pixel_shader(g_device, g_managed_resources.pixel_shaders["taa_0x505343B8"_h].put(), L"TAA_0x505343B8_ps.hlsl");
			}

			// Create MVs texture and RTV.
			[[unlikely]] if (!g_managed_resources.render_target_views["taa_0x505343B8"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["taa_0x505343B8"_h].put()), >= 0);
				ensure(g_device->CreateRenderTargetView(g_managed_resources.textures_2d["taa_0x505343B8"_h].get(), nullptr, g_managed_resources.render_target_views["taa_0x505343B8"_h].put()), >= 0);
			}

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["taa_0x505343B8"_h], nullptr);
			ctx->PSSetShader(g_managed_resources.pixel_shaders["taa_0x505343B8"_h].get(), nullptr, 0);

			cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

			//

			// DLSS pass
			//

			// Get SRVs and their resources.
			Com_ptr<ID3D11ShaderResourceView> srv_scene;
			ctx->PSGetShaderResources(4, 1, srv_scene.put());
			Com_ptr<ID3D11Resource> resource_scene;
			srv_scene->GetResource(resource_scene.put());
			Com_ptr<ID3D11ShaderResourceView> srv_depth;
			ctx->PSGetShaderResources(16, 1, srv_depth.put());
			Com_ptr<ID3D11Resource> resource_depth;
			srv_depth->GetResource(resource_depth.put());

			// Create the output resource for DLSS.
			[[unlikely]] if (!g_managed_resources.textures_2d["dlss_output"_h]) {
				ensure(resource_output->QueryInterface(g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
				D3D11_TEXTURE2D_DESC tex_desc;
				g_managed_resources.textures_2d["dlss_output"_h]->GetDesc(&tex_desc);
				tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
			}

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = resource_scene.get();
			eval_params.Feature.pInOutput = g_managed_resources.textures_2d["dlss_output"_h].get();
			eval_params.pInDepth = resource_depth.get();
			eval_params.pInMotionVectors = g_managed_resources.textures_2d["taa_0x505343B8"_h].get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			eval_params.InMVScaleX = g_swapchain_width;
			eval_params.InMVScaleY = g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in projection offsets so we need to scale them to pixel offsets for DLSS.
			eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * -0.5f;
			eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * 0.5f;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			// Copy DLSS output to the original TAA's current frame.
			ctx->CopyResource(resource_output.get(), g_managed_resources.textures_2d["dlss_output"_h].get());

			//

			return true;
		}
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
	hr = cs->GetPrivateData(g_cs_lightning_0x505343B8.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_lightning_0x505343B8.hash) {
		g_has_drawn_lightning = true;
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
				case g_ps_taa_0x505343B8.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_taa_0x505343B8.guid, sizeof(g_ps_taa_0x505343B8.hash), &g_ps_taa_0x505343B8.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_lightning_0x505343B8.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_lightning_0x505343B8.guid, sizeof(g_cs_lightning_0x505343B8.hash), &g_cs_lightning_0x505343B8.hash), >= 0);
					return;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	if (!g_has_drawn_lightning) {
		auto buffer = (ID3D11Buffer*)resource.handle;
		D3D11_BUFFER_DESC desc;
		buffer->GetDesc(&desc);
		if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 256) {
			g_mapped_cbs[resource.handle] = *data;
		}
	}

}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	// The CB we are after, from Lightning_0x0181192D_CS.
	//
	// cbuffer PER_BATCH : register(b0)
	// {
	//   float4 GiSettings : packoffset(c0);
	//   float4 TPLParams : packoffset(c1);
	//   float4 FrustumTL : packoffset(c2);
	//   float4 FrustumBL : packoffset(c3);
	//   float4 WorldViewPos : packoffset(c4);
	//   float4 PS_NearFarClipDist : packoffset(c5);
	//   float4 ProjParams : packoffset(c6); // ProjMatrix._m00, ProjMatrix._m11, ProjMatrix._m20, ProjMatrix._m21
	//   float4 ForwGiIntegrationMode : packoffset(c7);
	//   float4 SunDir : packoffset(c8);
	//   float4 PS_ScreenSize : packoffset(c9);
	//   float4 SSDOParams : packoffset(c10);
	//   float4 FrustumTR : packoffset(c11);
	//   float4 ScreenSize : packoffset(c12); // w, h, 1/w, 1/h
	//   float4 g_vVisAreasParams[64] : packoffset(c13);
	// }
	if (!g_has_drawn_lightning && g_mapped_cbs.contains(resource.handle)) {
		auto data = (float4*)g_mapped_cbs[resource.handle];

		// Base jitters (x, y) in range [-0.5, 0.5], for "r_AntialiasingTAAPattern 4" (the game setting).
		// ( 0.0625, -0.1875), (-0.0625,  0.1875),
		// ( 0.3125,  0.0625), (-0.1875, -0.3125),
		// (-0.3125,  0.3125), (-0.4375, -0.0625),
		// ( 0.1875,  0.4375), ( 0.4375, -0.4375)
		//
		// We only need to check for x jitters in range [-1, 1], y jitters will match.
		switch ((int)std::round(data[6].z * g_swapchain_width * 16.0f)) {
			case constexpr_round(0.0625f * 2.0f * 16.0f):
			case constexpr_round(0.3125f * 2.0f * 16.0f):
			case constexpr_round(-0.3125f * 2.0f * 16.0f):
			case constexpr_round(0.1875f * 2.0f * 16.0f):
			case constexpr_round(-0.0625f * 2.0f * 16.0f):
			case constexpr_round(-0.1875f * 2.0f * 16.0f):
			case constexpr_round(-0.4375f * 2.0f * 16.0f):
			case constexpr_round(0.4375f * 2.0f * 16.0f):
				g_jitter_x = data[6].z;
				g_jitter_y = data[6].w;
		}
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
			desc.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (resource_desc.texture.format == reshade::api::format::r16g16_snorm) {
			desc.format = reshade::api::format::r16g16_snorm;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}

		// Motion vectors.
		if (desc.texture.format == reshade::api::format::r8g8_snorm) {
			desc.texture.format = reshade::api::format::r16g16_snorm;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// Always force 16x.
		desc.max_anisotropy = 16.0f;

		// As recommended for DLAA.
		desc.mip_lod_bias += -1.0f;

		return true;
	}

	return false;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (g_force_modern_windowed && fullscreen) {
		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	#if 0
	return false;
	#endif

	if (g_force_modern_windowed) {
		//desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
		desc.back_buffer_count = std::max(2u, desc.back_buffer_count);
		desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.fullscreen_state = false;
	}

	if (g_force_vsync_off) {
		if (g_force_modern_windowed) {
			desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		desc.fullscreen_refresh_rate = 0.0f;
		desc.sync_interval = 0;
	}

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	native_swapchain->GetDesc(&desc);

	// Save device.
	g_device = (ID3D11Device*)swapchain->get_device()->get_native();

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		if (!resize) {
			DLSS::get_instance().init(g_device);
		}
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
	}

	// Reset resolution dependent resources.
	g_managed_resources.render_target_views["taa_0x505343B8"_h].reset();
	g_managed_resources.textures_2d["dlss_output"_h].reset();
}

static void on_destroy_device(reshade::api::device* device)
{
	if (device->get_native() != (uintptr_t)g_device) {
		return;
	}
	if (g_enable_dlss) {
		DLSS::get_instance().shutdown();
	}
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

	if (!reshade::get_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
	}
	if (!reshade::get_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
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

	if (ImGui::Checkbox("Force modern windowed", &g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Forces modern borderless or non borderless windowed mod.\nRequires restart.");
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
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
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::finish_present>(on_finish_present);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::map_buffer_region>(on_map_buffer_region);
			reshade::register_event<reshade::addon_event::unmap_buffer_region>(on_unmap_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
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
