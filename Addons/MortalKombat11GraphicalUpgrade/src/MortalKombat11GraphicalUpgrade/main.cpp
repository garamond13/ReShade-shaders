#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "MortalKombat11GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v2.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/MortalKombat11GraphicalUpgrade";

struct alignas(16) ViewConstants
{
	float4x4 _PRIVATE_ViewProjectionMatrix;
	float4x4 _PRIVATE_InvViewProjectionMatrix;
	float4x4 _PRIVATE_PreviousViewProjectionMatrix;
	float4x4 _PRIVATE_ClipToPrevClipMatrix;
	float4x4 _PRIVATE_ProjectionMatrix;
	float4x4 _PRIVATE_WorldToViewMatrix;
	float4x4 _PRIVATE_ViewToWorldMatrix;
	float4x4 _PRIVATE_InvProjectionMatrix;
	float4x4 _PRIVATE_PrevProjectionMatrix;
	float4x4 _PRIVATE_SceneToWorldMatrix;
	float4x4 _PRIVATE_ScreenToWorldMatrix;
	float4 _PRIVATE_ViewOrigin;
	float4 _PRIVATE_ScreenPositionScaleBias;
	float4 _PRIVATE_InvScreenPositionScaleBias;
	float4 _PRIVATE_TilePositionScaleBias;
	float4 _PRIVATE_WindowDimensions;
	float4 _PRIVATE_ScreenUVMinMax;
	float4 _PRIVATE_MinZ_MaxZRatio;
	float4 _PRIVATE_InvFocalLength;
	float4 _PRIVATE_DebugDirectIndirectEmissiveOverrides;
	float4 _PRIVATE_DebugDiffuseSpecularOverrides;
	float4 _PRIVATE_ExposureScales;
	
	struct
	{
		float4 Color;
		float4 DistanceDensity;
	} _PRIVATE_FogBandData[8];
	
	float3 _PRIVATE_ViewDirection;
	float _PRIVATE_AdaptiveTessellationFactor;
	uint _PRIVATE_ActiveDitherFrame;
	uint _PRIVATE_bUseHigherQualityGBufferEncoding;
	float _PRIVATE_ProjectionScaleX;
	float _PRIVATE_ProjectionScaleY;
	float3 _PRIVATE_VolumetricFogRange;
	/* bool */ uint _PRIVATE_VolumetricFogEnabled;
	float2 _PRIVATE_LevelDesatAndFadeControls;
	float _PRIVATE_AmbientIntensity;
	float _PRIVATE_AmbientAlpha;
	/* bool */ uint _PRIVATE_bDynamicPlanarReflectionsEnabled;
	/* bool */ uint _PRIVATE_bDynamicScreenSpaceReflectionsEnabled;
	int _PRIVATE_TotalEnvironmentMapVolumeCount;
	float _PRIVATE_SpecularMipFactor;
	float2 _PRIVATE_DOFFocusRange;
	float _PRIVATE_EnvironmentIBLContributionIntensity;
	uint _PRIVATE_dummy1;
	float _PRIVATE_DynamicResolutionScaleRatio;
	float _PRIVATE_InvDynamicResolutionScaleRatio;
	uint _PRIVATE_EnableTemporalDithering;
	uint _PRIVATE_const0_1;
	uint _PRIVATE_const0_2;
	uint _PRIVATE_const0_3;
	uint _PRIVATE_const0_4;
	uint _PRIVATE_const1_1;
	uint _PRIVATE_const1_2;
	uint _PRIVATE_const1_3;
	uint _PRIVATE_const1_4;
};

// Shader hooks.
//

constexpr Shader_hash g_cs_taa_0xA5BFCBC9 = { 0xA5BFCBC9, { 0x1b4167bb, 0xb4a3, 0x434e, { 0xa8, 0xee, 0x49, 0x3c, 0xae, 0xae, 0x34, 0x80 }}};
constexpr Shader_hash g_cs_taa_0xF529F5BE = { 0xF529F5BE, { 0xee3f6f43, 0xc3fb, 0x4b4c, { 0x80, 0x66, 0x58, 0xbd, 0x5d, 0x9f, 0x82, 0x71 }}};
constexpr Shader_hash g_cs_post_taa_sharpen_0xABAF5929 = { 0xABAF5929, { 0x5f43b193, 0xb4d3, 0x4a92, { 0x9c, 0x2f, 0x2d, 0x94, 0x61, 0x9a, 0x7f, 0xd8 }}};
constexpr Shader_hash g_cs_tonemap_0x70DD8EDC = { 0x70DD8EDC, { 0xbcd8fd0d, 0xcca7, 0x46dc, { 0x87, 0x11, 0xeb, 0x13, 0xba, 0xa4, 0xb5, 0xc7 }}};

//

static ID3D11Device* g_device;
static IDXGISwapChain* g_swapchain;
static Managed_resources g_managed_resources;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static bool g_hdr_fix;

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

static void draw_dlss(ID3D11DeviceContext* ctx)
{
	assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	std::array<ID3D11ShaderResourceView*, 4> srvs;
	ctx->CSGetShaderResources(0, srvs.size(), srvs.data());

	// Get depth resource.
	Com_ptr<ID3D11Resource> resource_depth;
	srvs[0]->GetResource(resource_depth.put());

	// Get scene resource.
	Com_ptr<ID3D11Resource> resource_scene;
	srvs[1]->GetResource(resource_scene.put());

	// Get MVs resource.
	Com_ptr<ID3D11Resource> resource_mvs;
	srvs[3]->GetResource(resource_mvs.put());

	// Get TAA resource.
	Com_ptr<ID3D11UnorderedAccessView> uav_taa;
	ctx->CSGetUnorderedAccessViews(1, 1, uav_taa.put());
	Com_ptr<ID3D11Resource> resource_taa;
	uav_taa->GetResource(resource_taa.put());

	// These need to be valid.
	assert(resource_depth);
	assert(resource_scene);
	assert(resource_mvs);
	assert(resource_taa);

	NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
	eval_params.Feature.pInColor = resource_scene.get();
	eval_params.Feature.pInOutput = resource_taa.get();
	eval_params.pInDepth = resource_depth.get();
	eval_params.pInMotionVectors = resource_mvs.get();

	// MVs are in UV space so we need to scale them to screen space for DLSS.
	// Also for DLSS we need to flip the sign for both x and y.
	eval_params.InMVScaleX = -g_swapchain_width;
	eval_params.InMVScaleY = -g_swapchain_height;

	eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
	eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

	// Jitters are in projection offsets so we need to rescale them to pixel offsets for DLSS.
	eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * -0.5;
	eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * 0.5;

	g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

	release_com_array(srvs);
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
	hr = cs->GetPrivateData(g_cs_taa_0xA5BFCBC9.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0xA5BFCBC9.hash) {
		if (g_enable_dlss) {
			draw_dlss(ctx);
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_taa_0xF529F5BE.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0xF529F5BE.hash) {
		if (g_enable_dlss) {
			draw_dlss(ctx);
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_post_taa_sharpen_0xABAF5929.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_post_taa_sharpen_0xABAF5929.hash) {
		if (g_enable_dlss) {
			// We can't just skip this draw.
			// SRV0 is the TAA out, and the UAV is empty and later read.

			// Get SRV resource.
			Com_ptr<ID3D11ShaderResourceView> srv;
			ctx->CSGetShaderResources(0, 1, srv.put());
			Com_ptr<ID3D11Resource> resource_srv;
			srv->GetResource(resource_srv.put());

			// Get UAV resource.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(0, 1, uav.put());
			Com_ptr<ID3D11Resource> resource_uav;
			uav->GetResource(resource_uav.put());

			ctx->CopyResource(resource_uav.get(), resource_srv.get());
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_tonemap_0x70DD8EDC.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_tonemap_0x70DD8EDC.hash) {
		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["tonemap_0x70DD8EDC"_h]) {
			create_compute_shader(g_device, g_managed_resources.compute_shaders["tonemap_0x70DD8EDC"_h].put(), L"Tonemap_0x70DD8EDC_cs.hlsl");
		}

		// Bindings.
		ctx->CSSetShader(g_managed_resources.compute_shaders["tonemap_0x70DD8EDC"_h].get(), nullptr, 0);

		return false;
	}

	return false;
}

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_taa_0xA5BFCBC9.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0xA5BFCBC9.guid, sizeof(g_cs_taa_0xA5BFCBC9.hash), &g_cs_taa_0xA5BFCBC9.hash), >= 0);
					return;
				case g_cs_taa_0xF529F5BE.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0xF529F5BE.guid, sizeof(g_cs_taa_0xF529F5BE.hash), &g_cs_taa_0xF529F5BE.hash), >= 0);
					return;
				case g_cs_post_taa_sharpen_0xABAF5929.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_post_taa_sharpen_0xABAF5929.guid, sizeof(g_cs_post_taa_sharpen_0xABAF5929.hash), &g_cs_post_taa_sharpen_0xABAF5929.hash), >= 0);
					return;
				case g_cs_tonemap_0x70DD8EDC.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_tonemap_0x70DD8EDC.guid, sizeof(g_cs_tonemap_0x70DD8EDC.hash), &g_cs_tonemap_0x70DD8EDC.hash), >= 0);
					return;
			}
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
		if (resource_desc.texture.format == reshade::api::format::r32g32_float) {
			desc.format = reshade::api::format::r32g32_float;
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
	// ViewConstants CB is immutable so we have to catch it on creation.
	// The game will be recreating it at least onece per frame.
	if ((desc.usage & reshade::api::resource_usage::constant_buffer) != 0 && (desc.flags & reshade::api::resource_flags::immutable) != 0 && desc.buffer.size == 1280) {
		auto data = (ViewConstants*)initial_data->data;

		// This should be reliable.
		if (data->_PRIVATE_ProjectionMatrix.m00 && !data->_PRIVATE_ProjectionMatrix.m01 && !data->_PRIVATE_ProjectionMatrix.m02 && !data->_PRIVATE_ProjectionMatrix.m03 && !data->_PRIVATE_ProjectionMatrix.m10 && data->_PRIVATE_ProjectionMatrix.m11 && data->_PRIVATE_ProjectionMatrix.m23 == 1.0f) {
			g_jitter_x = data->_PRIVATE_ProjectionMatrix.m20;
			g_jitter_y = data->_PRIVATE_ProjectionMatrix.m21;
		}

		return false;
	}

	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}

		// Motion vecotrs.
		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}

		// Depth.
		if (desc.texture.format == reshade::api::format::r16_float) {
			desc.texture.format = reshade::api::format::r32_float;
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
	g_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	g_swapchain->GetDesc(&desc);

	// Save device.
	g_device = (ID3D11Device*)swapchain->get_device()->get_native();

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_hdr_fix) {
		Com_ptr<IDXGISwapChain3> swapchain3;
		ensure(g_swapchain->QueryInterface(swapchain3.put()), >= 0);
		swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
	}

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		if (!resize) {
			DLSS::get_instance().init(g_device);
		}
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
	}
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime)
{
	if (g_hdr_fix) {
		runtime->set_color_space(reshade::api::color_space::hdr10_st2084);
	}
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
	if (!reshade::get_config_value(nullptr, NAME, "HDRFix", g_hdr_fix)) {
		reshade::set_config_value(nullptr, NAME, "HDRFix", g_hdr_fix);
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

	if (ImGui::Checkbox("HDR fix", &g_hdr_fix)) {
		Com_ptr<IDXGISwapChain3> swapchain3;
		ensure(g_swapchain->QueryInterface(swapchain3.put()), >= 0);
		if (g_hdr_fix) {
			swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
			runtime->set_color_space(reshade::api::color_space::hdr10_st2084);
		}
		else {
			swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
			runtime->set_color_space(reshade::api::color_space::srgb);
		}
		reshade::set_config_value(nullptr, NAME, "HDRFix", g_hdr_fix);
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
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
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
