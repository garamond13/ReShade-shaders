#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "TheEvilWithin2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/TheEvilWithin2GraphicalUpgrade";

struct alignas(16) fblock
{
	float4 renderpositiontoviewtexture;
	float4 lightprepassinverseparams;
	float4 prevviewposition;
	float4 prevviewprojectionmatrixrcx;
	float4 prevviewprojectionmatrixrcy;
	float4 prevviewprojectionmatrixrcw;
	float4 projectionmatrixz;
	float4 blurstep;
	float4 motionblurparms;
	float4 texsize;
	float4 tsaaparm;
};

// Shader hooks.
//

constexpr Shader_hash g_ps_taa_0xB57DD4D6 = { 0xB57DD4D6, { 0x6d16baf8, 0x6df, 0x4e58, { 0x84, 0x62, 0xcb, 0x65, 0xb9, 0x6a, 0x68, 0x53 }}};
constexpr Shader_hash g_ps_tonemap_0xB13F7764 = { 0xB13F7764, { 0xa03d5123, 0xb90c, 0x4e1d, { 0xa3, 0x37, 0x79, 0xb2, 0x22, 0x4f, 0x6b, 0xc3 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D11Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
uintptr_t g_mapped_cb_handle;
void* g_mapped_cb_data;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static bool g_disable_lens_dirt = true;

// DLSS
constexpr int g_dlss_flags{
	NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
	NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
};
static NVSDK_NGX_DLSS_Hint_Render_Preset g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
static int g_user_set_dlss_preset;
static bool g_enable_dlss;
static bool g_dlss_status;
static float g_jitter_x;
static float g_jitter_y;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

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
	hr = ps->GetPrivateData(g_ps_taa_0xB57DD4D6.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_taa_0xB57DD4D6.hash) {
		if (g_enable_dlss) {
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// Get SRVs.
			std::array<ID3D11ShaderResourceView*, 3> srvs;
			ctx->PSGetShaderResources(0, srvs.size(), srvs.data());

			// Get SRV0 resource, depth.
			Com_ptr<ID3D11Resource> resource_depth;
			srvs[0]->GetResource(resource_depth.put());

			// Get SRV2 resource, scene.
			Com_ptr<ID3D11Resource> resource_scene;
			srvs[2]->GetResource(resource_scene.put());

			// Get RTVs.
			std::array<ID3D11RenderTargetView*, 3> rtvs;
			ctx->OMGetRenderTargets(rtvs.size(), rtvs.data(), nullptr);

			// Get RTV1 resource, TAA out.
			Com_ptr<ID3D11Resource> resource_rt;
			rtvs[1]->GetResource(resource_rt.put());

			// MVs pass
			//

			// Create PS.
			[[unlikely]] if (!g_managed_resources.pixel_shaders["TAA_0xB57DD4D6"_h]) {
				create_pixel_shader(g_device, g_managed_resources.pixel_shaders["TAA_0xB57DD4D6"_h].put(), L"TAA_0xB57DD4D6_ps.hlsl");
			}

			// Create RTV.
			if (!g_managed_resources.render_target_views["mvs"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["mvs"_h].put()), >= 0);
				ensure(g_device->CreateRenderTargetView(g_managed_resources.textures_2d["mvs"_h].get(), nullptr, g_managed_resources.render_target_views["mvs"_h].put()), >= 0);
			}

			// Bindings.
			const std::array mvs_pass_rtvs = { g_managed_resources.render_target_views["mvs"_h].get(), rtvs[2] };
			ctx->OMSetRenderTargets(mvs_pass_rtvs.size(), mvs_pass_rtvs.data(), nullptr);
			ctx->PSSetShader(g_managed_resources.pixel_shaders["TAA_0xB57DD4D6"_h].get(), nullptr, 0);

			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

			//

			// Create texture.
			// The original RT resource doesn't have D3D11_BIND_UNORDERED_ACCESS bind flag, needed for DLSS.
			[[unlikely]] if (!g_managed_resources.textures_2d["dlss_output"_h]) {
				// Get original RT texture description.
				ensure(resource_rt->QueryInterface(g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
				D3D11_TEXTURE2D_DESC tex_desc;
				g_managed_resources.textures_2d["dlss_output"_h]->GetDesc(&tex_desc);

				// Create DLSS output.
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
			}

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = resource_scene.get();
			eval_params.Feature.pInOutput = g_managed_resources.textures_2d["dlss_output"_h].get();
			eval_params.pInDepth = resource_depth.get();
			eval_params.pInMotionVectors = g_managed_resources.textures_2d["mvs"_h].get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			eval_params.InMVScaleX = -g_swapchain_width;
			eval_params.InMVScaleY = -g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters.
			eval_params.InJitterOffsetX = g_jitter_x * -1.0f;
			eval_params.InJitterOffsetY = g_jitter_y * 1.0f;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			// Copy DLSS output to the original output.
			ctx->CopyResource(resource_rt.get(), g_managed_resources.textures_2d["dlss_output"_h].get());

			release_com_array(rtvs);
			release_com_array(srvs);
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xB13F7764.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xB13F7764.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["Tonemap_0xB13F7764"_h]) {
			std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["Tonemap_0xB13F7764"_h].put(), L"Tonemap_0xB13F7764_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["Tonemap_0xB13F7764"_h].get(), nullptr, 0);

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
				case g_ps_taa_0xB57DD4D6.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_taa_0xB57DD4D6.guid, sizeof(g_ps_taa_0xB57DD4D6.hash), &g_ps_taa_0xB57DD4D6.hash), >= 0);
					return;
				case g_ps_tonemap_0xB13F7764.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xB13F7764.guid, sizeof(g_ps_tonemap_0xB13F7764.hash), &g_ps_tonemap_0xB13F7764.hash), >= 0);
					return;
			}
		}
	}
}

static bool on_update_buffer_region(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
{
	// This should be reliable.
	if (size >= sizeof(fblock)) {
		g_jitter_x = ((fblock*)data)->tsaaparm.y;
		g_jitter_y = ((fblock*)data)->tsaaparm.z;
	}

	return false;
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
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
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
		// Max anisotropy used by the game is 4x?
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
		desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
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
	g_managed_resources.textures_2d["dlss_output"_h].reset();
	g_managed_resources.render_target_views["mvs"_h].reset();
}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	// Set maximum frame latency to 1.
	auto native_device = (ID3D11Device*)device->get_native();
	Com_ptr<IDXGIDevice1> dxgi_device;
	auto hr = native_device->QueryInterface(dxgi_device.put());
	if (SUCCEEDED(hr)) {
		ensure(dxgi_device->SetMaximumFrameLatency(1), >= 0);
	}
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

	if (!reshade::get_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt);
	}
	if (!reshade::get_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
	}
	if (!reshade::get_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}

	if (!reshade::get_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);

	if (!reshade::get_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
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

	if (ImGui::Checkbox("Disable lens dirt", &g_disable_lens_dirt)) {
		g_managed_resources.pixel_shaders["Tonemap_0xB13F7764"_h].reset();
		reshade::set_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt);
	}
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

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
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
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::update_buffer_region>(on_update_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
