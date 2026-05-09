#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS.h"

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

static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_borderless = true;
static bool g_hdr_fix;
static IDXGISwapChain* g_swapchain;

// DLSS.
static bool g_enable_dlss;
static DLSS_PRESET g_dlss_preset = DLSS_PRESET_F;
static uintptr_t g_dlss_device;
static float g_jitter_x;
static float g_jitter_y;

static void draw_dlss(ID3D11DeviceContext* ctx)
{
	assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	// Get depth resource.
	Com_ptr<ID3D11ShaderResourceView> srv_stencil;
	ctx->CSGetShaderResources(0, 1, srv_stencil.put());
	Com_ptr<ID3D11Resource> resource_depth;
	srv_stencil->GetResource(resource_depth.put());

	// Get scene resource.
	Com_ptr<ID3D11ShaderResourceView> srv_scene;
	ctx->CSGetShaderResources(1, 1, srv_scene.put());
	Com_ptr<ID3D11Resource> resource_scene;
	srv_scene->GetResource(resource_scene.put());

	// Get MVs resource.
	Com_ptr<ID3D11ShaderResourceView> srv_mvs;
	ctx->CSGetShaderResources(3, 1, srv_mvs.put());
	Com_ptr<ID3D11Resource> resource_mvs;
	srv_mvs->GetResource(resource_mvs.put());

	// Get TAA resource.
	Com_ptr<ID3D11Resource> resource_taa;
	Com_ptr<ID3D11UnorderedAccessView> uav_taa;
	ctx->CSGetUnorderedAccessViews(1, 1, uav_taa.put());
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

	// Jitters are in NDC offsets so we need to scale them to pixel offsets for DLSS.
	eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * -1.0;
	eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * 1.0;

	DLSS::instance().draw(ctx, eval_params);
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
	auto hr = cs->GetPrivateData(g_cs_taa_0xA5BFCBC9.guid, &size, &hash);
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

			// Just copy the scene and skip the original draw.
			ctx->CopyResource(resource_uav.get(), resource_srv.get());
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_tonemap_0x70DD8EDC.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_tonemap_0x70DD8EDC.hash) {
		// Create PS.
		[[unlikely]] if (!g_cs["tonemap_0x70DD8EDC"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			create_compute_shader(device.get(), g_cs["tonemap_0x70DD8EDC"_h].put(), L"Tonemap_0x70DD8EDC_cs.hlsl");
		}
		ctx->CSSetShader(g_cs["tonemap_0x70DD8EDC"_h].get(), nullptr, 0);
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
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0xA5BFCBC9.guid, sizeof(g_cs_taa_0xA5BFCBC9.hash), &g_cs_taa_0xA5BFCBC9.hash), >= 0);
					return;
				case g_cs_taa_0xF529F5BE.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0xF529F5BE.guid, sizeof(g_cs_taa_0xF529F5BE.hash), &g_cs_taa_0xF529F5BE.hash), >= 0);
					return;
				case g_cs_post_taa_sharpen_0xABAF5929.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_cs_post_taa_sharpen_0xABAF5929.guid, sizeof(g_cs_post_taa_sharpen_0xABAF5929.hash), &g_cs_post_taa_sharpen_0xABAF5929.hash), >= 0);
					return;
				case g_cs_tonemap_0x70DD8EDC.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_cs_tonemap_0x70DD8EDC.guid, sizeof(g_cs_tonemap_0x70DD8EDC.hash), &g_cs_tonemap_0x70DD8EDC.hash), >= 0);
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
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_unorm) {
			desc.format = reshade::api::format::r16g16b16a16_unorm;
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
		if (resource_desc.texture.format == reshade::api::format::r16_unorm) {
			desc.format = reshade::api::format::r16_unorm;
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
	// Upgrading r10g10b10a2_unorm breaks HDR.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm || desc.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16_float) {
			desc.texture.format = reshade::api::format::r32_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8_unorm) {
			desc.texture.format = reshade::api::format::r16_unorm;
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
	if (g_force_borderless && fullscreen) {
		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	#if 0
	return false;
	#endif

	if (g_force_borderless) {
		desc.back_buffer_count = std::max(2u, desc.back_buffer_count);
		desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.fullscreen_state = false;
	}

	if (g_force_vsync_off) {
		desc.present_flags |= g_force_borderless ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
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

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_hdr_fix) {
		Com_ptr<IDXGISwapChain3> swapchain3;
		ensure(g_swapchain->QueryInterface(swapchain3.put()), >= 0);
		swapchain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
	}

	if (g_enable_dlss) {
		Com_ptr<ID3D11Device> device;
		ensure(g_swapchain->GetDevice(IID_PPV_ARGS(device.put())), >= 0);
		Com_ptr<ID3D11DeviceContext> ctx;
		device->GetImmediateContext(ctx.put());
		DLSS::instance().init(device.get());
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
		g_dlss_device = (uintptr_t)device.get();
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
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(device1.put());
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void on_destroy_device(reshade::api::device* device)
{
	if (g_enable_dlss && device->get_native() == g_dlss_device) {
		DLSS::instance().shutdown();
		g_dlss_device = 0;
	}

	clear_device_resources();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	if (!reshade::get_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "DLSSPreset", g_dlss_preset)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "DLSSPreset", g_dlss_preset);
	}
	if (!reshade::get_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceBorderless", g_force_borderless)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceBorderless", g_force_borderless);
	}
	if (!reshade::get_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "HDRFix", g_hdr_fix)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "HDRFix", g_hdr_fix);
	}
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

	// The game may set this a bit later.
	if (ImGui::Button("Check MaximumFrameLatency")) {
		Com_ptr<IDXGIDevice1> device1;
		ensure(device->QueryInterface(device1.put()), >= 0);
		UINT max_latency;
		ensure(device1->GetMaximumFrameLatency(&max_latency), >= 0);
		log_debug("MaximumFrameLatency: {}", max_latency);
	}
	ImGui::NewLine();
	#endif

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			DLSS::instance().init(device);
			DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
			g_dlss_device = (uintptr_t)device;
		}
		else {
			DLSS::instance().shutdown();
			g_dlss_device = 0;
		}
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "E", "F", "K", "L", "M" };
	if (ImGui::Combo("DLSS preset", &g_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "DLSSPreset", g_dlss_preset);
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();
	
	if (ImGui::Checkbox("Force borderless", &g_force_borderless)) {
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "ForceBorderless", g_force_borderless);
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
		reshade::set_config_value(nullptr, "MortalKombat11GraphicalUpgrade", "HDRFix", g_hdr_fix);
	}
	ImGui::Spacing();
}

extern "C" __declspec(dllexport) const char* NAME = "MortalKombat11GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "MortalKombat11GraphicalUpgrade v1.1.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/MortalKombat11GraphicalUpgrade";

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
