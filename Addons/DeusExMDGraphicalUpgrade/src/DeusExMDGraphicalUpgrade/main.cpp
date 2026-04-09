#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Helpers.h"
#include "TRC.h"
#include "minhook\include\MinHook.h"
#include "DLSS.h"
#include "HLSLTypes.h"

struct alignas(16) cbSharedPerViewData
{
    float4x4    mProjection;
    float4x4    mProjectionInverse;
    float4x4    mViewToViewport;
    float4x4    mWorldToView;
    float4x4    mViewToWorld;
    float4    vViewRemap;
    float4    vViewDepthRemap;
    float4    vEyeVectorUL;
    float4    vEyeVectorLR;
    float4    vViewSpaceUpVector;
    float4    vCheckerModeParams;
    float4    vViewportSize;
    float4    vEngineTime;
    float4    vPrecipitations;
    float4    vClipPlane;
    float4    vExtraParams;
    float4    vScatteringParams;
    float4    vStereoscopic3DCorrectionParams;
    float4    vCHSParams;
};

// Shader hooks and replacemnt shaders.
//

constexpr uint32_t g_cs_taa_0x84EF14ED_hash = 0x84EF14ED;
constexpr GUID g_cs_taa_0x84EF14ED_guid = { 0xd8f2c861, 0x3dfe, 0x48e1, { 0x8c, 0xd, 0xb4, 0x3d, 0x34, 0xfe, 0x77, 0x5b } };

constexpr uint32_t g_ps_sharpen_0xDA65F8ED_hash = 0xDA65F8ED;
constexpr GUID g_ps_sharpen_0xDA65F8ED_guid = { 0xf321cf9a, 0xb180, 0x4189, { 0xbb, 0x5d, 0x85, 0x27, 0x85, 0x6e, 0x9b, 0xd9 } };

constexpr uint32_t g_ps_last_known_location_0xE30A40E6_hash = 0xE30A40E6;
constexpr GUID g_ps_last_known_location_0xE30A40E6_guid = { 0x53534603, 0xccd7, 0x43e1, { 0x8c, 0x25, 0x6f, 0x6b, 0xb9, 0x1, 0x3f, 0x4c } };

constexpr uint32_t g_ps_last_known_location_0xA6A0E453_hash = 0xA6A0E453;
constexpr GUID g_ps_last_known_location_0xA6A0E453_guid = { 0x76699d13, 0x2d88, 0x4601, { 0xa7, 0x6a, 0xe0, 0x27, 0xb7, 0x22, 0x9b, 0x27 } };

//

static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static float g_amd_ffx_cas_sharpness = 0.4f;
static bool g_disable_last_known_location = true;

// DLSS.
static ID3D11Device* g_dlss_device;
static bool g_enable_dlss = false;
static DLSS_PRESET g_dlss_preset = DLSS_PRESET_F;

// Jitters.
void* g_cbSharedPerViewData_mapped_data;
uintptr_t g_cbSharedPerViewData_handle;
cbSharedPerViewData g_cbSharedPerViewData_data;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

// We need to change flags on present, ReShade's callback can't handle that.
typedef HRESULT(__stdcall *IDXGISwapChain__Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static IDXGISwapChain__Present g_original_present;

// COM resources.
static std::unordered_map<uint32_t, Com_ptr<ID3D11ComputeShader>> g_cs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11PixelShader>> g_ps;

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	// The game is doing fake preset with DXGI_PRESENT_TEST flag
	// than right after does the actual present.
	// DXGI_PRESENT_ALLOW_TEARING may be set by ReShade,
	// it's incompatible with DXGI_PRESENT_TEST so we have to remove it.
	if (flags & DXGI_PRESENT_TEST) {
		// Limit FPS
		//

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

		//

		flags &= ~DXGI_PRESENT_ALLOW_TEARING;
	}

	auto hr = g_original_present(swapchain, sync_interval, flags);

	return hr;
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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_sharpen_0xDA65F8ED_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_sharpen_0xDA65F8ED_hash) {
		// Create CS.
		[[unlikely]] if (!g_ps[hash_name("sharpen_0xDA65F8ED")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("sharpen_0xDA65F8ED")].put(), L"Sharpen_0xDA65F8ED_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("sharpen_0xDA65F8ED")].get(), nullptr, 0);

		return false;
	}

	return false;
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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_last_known_location_0xA6A0E453_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_last_known_location_0xA6A0E453_hash) {
		if (g_disable_last_known_location) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_last_known_location_0xE30A40E6_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_last_known_location_0xE30A40E6_hash) {
		if (g_disable_last_known_location) {
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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = cs->GetPrivateData(g_cs_taa_0x84EF14ED_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0x84EF14ED_hash) {
		if (g_enable_dlss) {
			// DLSS requires immediate context.
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// DLSS pass
			//
			// The game renders all in sRGB (or mixed), should we linearize for DLSS?
			//

			// Get SRVs and their resources.
			std::array<ID3D11ShaderResourceView*, 3> srvs = {};
			ctx->CSGetShaderResources(0, srvs.size(), srvs.data());
			Com_ptr<ID3D11Resource> depth;
			srvs[0]->GetResource(depth.put());
			Com_ptr<ID3D11Resource> scene;
			srvs[1]->GetResource(scene.put());
			Com_ptr<ID3D11Resource> mvs;
			srvs[2]->GetResource(mvs.put());

			// Get UAV and it's resource.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(0, 1, uav.put());
			Com_ptr<ID3D11Resource> output;
			uav->GetResource(output.put());

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = scene.get();
			eval_params.Feature.pInOutput = output.get();
			eval_params.pInDepth = depth.get();
			eval_params.pInMotionVectors = mvs.get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			// Also for DLSS we need to flip the sign for both x and y.
			eval_params.InMVScaleX = -g_swapchain_width;
			eval_params.InMVScaleY = -g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
			eval_params.InJitterOffsetX = g_cbSharedPerViewData_data.mProjection.m[2][0] * (float)g_swapchain_width * -0.5f;
			eval_params.InJitterOffsetY = g_cbSharedPerViewData_data.mProjection.m[2][1] * (float)g_swapchain_height * 0.5f;

			DLSS::instance().draw(ctx, eval_params);

			//

			// PostDLSS pass
			//

			// Create CS.
			[[unlikely]] if (!g_cs[hash_name("post_dlss")]) {
				Com_ptr<ID3D11Device> device;
				ctx->GetDevice(device.put());
				create_compute_shader(device.get(), g_cs[hash_name("post_dlss")].put(), L"PostDLSS_cs.hlsl");
			}

			// Bindings.
			ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			ctx->CSSetShader(g_cs[hash_name("post_dlss")].get(), nullptr, 0);

			ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);
			
			//

			return true;
		}

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
				case g_ps_sharpen_0xDA65F8ED_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_ps_sharpen_0xDA65F8ED_guid, sizeof(g_ps_sharpen_0xDA65F8ED_hash), &g_ps_sharpen_0xDA65F8ED_hash), >= 0);
					break;
				case g_ps_last_known_location_0xA6A0E453_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_ps_last_known_location_0xA6A0E453_guid, sizeof(g_ps_last_known_location_0xA6A0E453_hash), &g_ps_last_known_location_0xA6A0E453_hash), >= 0);
					break;
				case g_ps_last_known_location_0xE30A40E6_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_ps_last_known_location_0xE30A40E6_guid, sizeof(g_ps_last_known_location_0xE30A40E6_hash), &g_ps_last_known_location_0xE30A40E6_hash), >= 0);
					break;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_taa_0x84EF14ED_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0x84EF14ED_guid, sizeof(g_cs_taa_0x84EF14ED_hash), &g_cs_taa_0x84EF14ED_hash), >= 0);
					break;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	D3D11_BUFFER_DESC desc;
	buffer->GetDesc(&desc);
	if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 544) {
		g_cbSharedPerViewData_handle = resource.handle;
		g_cbSharedPerViewData_mapped_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	if (g_cbSharedPerViewData_handle == resource.handle) {
		auto data = (float*)g_cbSharedPerViewData_mapped_data;

		// This should be reliable.
		if (data[8] && data[9] && data[8] == data[129]) {
			std::memcpy(&g_cbSharedPerViewData_data, data, sizeof(g_cbSharedPerViewData_data));
			g_cbSharedPerViewData_handle = 0;
		}
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		// Back buffer.
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_unorm) {
			desc.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
			desc.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (resource_desc.texture.format == reshade::api::format::r32_float) {
			desc.format = reshade::api::format::r32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm || desc.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16_float) {
			desc.texture.format = reshade::api::format::r32_float;
			return true;
		}
	}

	return false;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	#if 0
	return false;
	#endif

	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = std::max(2u, desc.back_buffer_count);
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.fullscreen_state = false;

	if (g_force_vsync_off) {
		desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
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

	// Hook IDXGISwapChain::Present
	if (!g_original_present) {
		auto vtable = *(void***)native_swapchain;
		ensure(MH_CreateHook(vtable[8], &detour_present, (void**)&g_original_present), == MH_OK);
		ensure(MH_EnableHook(vtable[8]), == MH_OK);
	}

	// Minimum supported resolution by DLSS.
	// The game may create the 2nd swapchain that should be 2x2.
	if (desc.BufferDesc.Width < 32 && desc.BufferDesc.Height < 32) {
		return;
	}

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_enable_dlss) {
		Com_ptr<ID3D11Device> device;
		ensure(native_swapchain->GetDevice(IID_PPV_ARGS(device.put())), >= 0);
		Com_ptr<ID3D11DeviceContext> ctx;
		device->GetImmediateContext(ctx.put());
		DLSS::instance().init(device.get());
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
		g_dlss_device = device.get(); // We just assume that this is correct which is not good, but what is good in this clusterfuck? 
	}
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	auto native_device = (ID3D11Device*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(device1.put());
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void on_destroy_device(reshade::api::device* device)
{
	if (g_enable_dlss && (ID3D11Device*)device->get_native() == g_dlss_device) {
		DLSS::instance().shutdown();
		g_dlss_device = nullptr;
	}
	g_cs.clear();
	g_ps.clear();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DLSSPreset", g_dlss_preset)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DLSSPreset", g_dlss_preset);
	}
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DisableLastKnownLocation", g_disable_last_known_location)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DisableLastKnownLocation", g_disable_last_known_location);
	}
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "DeusExMDGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	auto device = (ID3D11Device*)runtime->get_device()->get_native();
	Com_ptr<ID3D11DeviceContext> ctx;
	device->GetImmediateContext(ctx.put());

	#if DEV
	if (ImGui::Button("Dev button")) {
	}
	ImGui::Spacing();
	#endif

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			DLSS::instance().init(device);
			DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
			g_dlss_device = device; // We just assume that this is correct which is not good, but what is good in this clusterfuck? 
		}
		else {
			DLSS::instance().shutdown();
			g_dlss_device = nullptr;
		}
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "F", "K" };
	if (ImGui::Combo("DLSS preset", &g_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DLSSPreset", g_dlss_preset);
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable last known location marker", &g_disable_last_known_location)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "DisableLastKnownLocation", g_disable_last_known_location);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps[hash_name("sharpen_0xDA65F8ED")].reset();
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "DeusExMDGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "DeusExMDGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "DeusExMDGraphicalUpgrade v1.2.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/DeusExMDGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			g_graphical_upgrade_path = get_graphical_upgrade_path();
			read_config();
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::map_buffer_region>(on_map_buffer_region);
			reshade::register_event<reshade::addon_event::unmap_buffer_region>(on_unmap_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			MH_Uninitialize();
			break;
	}
	return TRUE;
}