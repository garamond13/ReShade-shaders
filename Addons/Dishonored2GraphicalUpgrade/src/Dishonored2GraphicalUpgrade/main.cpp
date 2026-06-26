#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "Dishonored2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v4.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/Dishonored2GraphicalUpgrade";

struct alignas(16) PerViewCB
{
	float4 cb_alwaystweak;
	float4 cb_viewrandom;
	float4x4 cb_viewprojectionmatrix;
	float4x4 cb_viewmatrix;
	float4 cb_subpixeloffset;
	float4x4 cb_projectionmatrix;
	float4x4 cb_previousviewprojectionmatrix;
	float4x4 cb_previousviewmatrix;
	float4x4 cb_previousprojectionmatrix;
	float4 cb_mousecursorposition;
	float4 cb_mousebuttonsdown;
	float4 cb_jittervectors;
	float4x4 cb_inverseviewprojectionmatrix;
	float4x4 cb_inverseviewmatrix;
	float4x4 cb_inverseprojectionmatrix;
	float4 cb_globalviewinfos;
	float3 cb_wscamforwarddir;
	uint cb_alwaysone;
	float3 cb_wscamupdir;
	uint cb_usecompressedhdrbuffers;
	float3 cb_wscampos;
	float cb_time;
	float3 cb_wscamleftdir;
	float cb_systime;
	float2 cb_jitterrelativetopreviousframe;
	float2 cb_worldtime;
	float2 cb_shadowmapatlasslicedimensions;
	float2 cb_resolutionscale;
	float2 cb_parallelshadowmapslicedimensions;
	float cb_framenumber;
	uint cb_alwayszero;
};

// Shader hooks.
//

constexpr Shader_hash g_ps_ssao_0x94445D2D = { 0x94445D2D, { 0x37961341, 0x530f, 0x405b, { 0xa7, 0x38, 0xdf, 0xc2, 0x5c, 0xaa, 0x32, 0xdc }}};
constexpr Shader_hash g_ps_tonemap_0xA6F33860 = { 0xA6F33860, { 0xdd2785dc, 0x1465, 0x418a, { 0xa9, 0xaf, 0xe2, 0x4f, 0x1e, 0xd0, 0x4c, 0x29 }}};
constexpr Shader_hash g_ps_tonemap_0x11E16EF3 = { 0x11E16EF3, { 0xd0922fff, 0x1053, 0x46f8, { 0xa0, 0x91, 0x11, 0xd9, 0xa3, 0x4a, 0xdd, 0x89 }}};
constexpr Shader_hash g_ps_vignette_0xFD4216EA = { 0xFD4216EA, { 0x96c298b8, 0xd56a, 0x4e67, { 0x90, 0x12, 0x8, 0x8a, 0xeb, 0x99, 0xb1, 0xf }}};
constexpr Shader_hash g_ps_upsample_0x1A0CD2AE = { 0x1A0CD2AE, { 0xaaf2a4bb, 0x496b, 0x47be, { 0xbf, 0xf6, 0xae, 0x48, 0xb3, 0x3f, 0x6f, 0x50 }}};
constexpr Shader_hash g_cs_downsample_depth_0x27BD5265 = { 0x27BD5265, { 0xd2d58efa, 0x76e4, 0x45a6, { 0xae, 0x81, 0x46, 0x83, 0xd0, 0x11, 0x86, 0x28 }}};
constexpr Shader_hash g_cs_taa_0x06BBC941 = { 0x06BBC941, { 0x16cb60d, 0x938, 0x4e9d, { 0xa7, 0x7f, 0xea, 0x40, 0x56, 0x19, 0x9, 0xd6 }}};

// If motion blur is enabled in the game's settings.
constexpr Shader_hash g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8 = { 0xC97890C8, { 0x51783160, 0x2604, 0x4d2e, { 0x91, 0xf2, 0x68, 0x35, 0x58, 0x7a, 0xf6, 0xb1 }}};

// If motion blur is disabled in the game's settings.
constexpr Shader_hash g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905 = { 0xE908E905, { 0x337d2791, 0xda14, 0x48f1, { 0x85, 0x2d, 0x5b, 0xcc, 0x3, 0x7e, 0xfa, 0x60 }}};

constexpr Shader_hash g_ps_denoise_0x4CD2BE39 = { 0x4CD2BE39, { 0x1cbed7cf, 0xf, 0x4e33, { 0xaf, 0x2c, 0x6a, 0x45, 0x88, 0x7d, 0xd9, 0xd8 }}};
constexpr Shader_hash g_ps_downsample_0x42873B15 = { 0x42873B15, { 0xa40a0d2e, 0x6ab1, 0x4ff1, { 0x9e, 0x51, 0x6f, 0xbd, 0xf5, 0xaa, 0x6f, 0x5a }}};

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
static std::atomic<uintptr_t> g_taa_tag;
static void* g_taa_cb1_mapped_data;
static bool g_disable_lens_dirt = true;
static float g_vignette_strenght = 1.0f;
static float g_bloom_intensity = 1.0f;
static bool g_disable_lens_distortion;
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

// VB-GTAO
constexpr size_t VB_GTAO_DEPTH_MIP_LEVELS = 5;
static int g_vb_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// Device resources.
static std::array<ID3D11UnorderedAccessView*, VB_GTAO_DEPTH_MIP_LEVELS> g_uav_vb_gtao_prefilter_depths16x16;

// This gets called on both D3D11DeviceContext::FinishCommandList and D3D11DeviceContext::ExecuteCommandList as:
// on_FinishCommandList(ID3D11CommandList*, (D3D11DeviceContext*)this) and on_ExecuteCommandList((D3D11DeviceContext*)this, ID3D11CommandList*).
static void on_execute_secondary_command_list(reshade::api::command_list* cmd_list, reshade::api::command_list* secondary_cmd_list)
{
	if (!g_enable_dlss) {
		return;
	}

	// Are we in D3D11DeviceContext::FinishCommandList
	Com_ptr<ID3D11DeviceContext> ctx;
	auto unknown = (IUnknown*)(secondary_cmd_list->get_native());
	auto hr = unknown->QueryInterface(ctx.put());
	if (SUCCEEDED(hr)) {
		if (g_taa_tag == (uintptr_t)ctx.get()) {
			g_taa_tag = cmd_list->get_native();
		}
		return;
	}

	// At this point we are in D3D11DeviceContext::ExecuteCommandList
	if (g_taa_tag == secondary_cmd_list->get_native()) {
		g_taa_tag = 0;
		ctx = (ID3D11DeviceContext*)(cmd_list->get_native());

		// PreDLSS pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["pre_dlss"_h]) {
			create_compute_shader(g_device, g_managed_resources.compute_shaders["pre_dlss"_h].put(), L"PreDLSS_cs.hlsl");
		}

		// Create UAV.
		ensure(g_device->CreateUnorderedAccessView(g_managed_resources.resources["scene"_h].get(), nullptr, g_managed_resources.unordered_access_views["scene"_h].put()), >= 0);

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["scene"_h], nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["pre_dlss"_h].get(), nullptr, 0);
		const std::array pre_dlss_cbs = { g_managed_resources.buffers["taa_b1"_h].get(), g_managed_resources.buffers["taa_b2"_h].get() };
		ctx->CSSetConstantBuffers(1, pre_dlss_cbs.size(), pre_dlss_cbs.data());
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["ro_postfx_luminance_buffautoexposure"_h]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);
		
		//

		// DLSS pass
		//

		NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
		eval_params.Feature.pInColor = g_managed_resources.resources["scene"_h].get();
		eval_params.Feature.pInOutput = g_managed_resources.resources["taa"_h].get();
		eval_params.pInDepth = g_managed_resources.resources["depth"_h].get();
		eval_params.pInMotionVectors = g_managed_resources.resources["mvs"_h].get();

		// MVs are in UV space so we need to scale them to screen space for DLSS.
		eval_params.InMVScaleX = -g_swapchain_width;
		eval_params.InMVScaleY = -g_swapchain_height;

		eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
		eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

		// Jitters are in NDC offsets so we need to scale them to pixel offsets for DLSS.
		eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * -0.5f;
		eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * 0.5f;

		g_dlss_status = DLSS::get_instance().draw(ctx.get(), eval_params);

		//

		// PostDLSS pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["post_dlss"_h]) {
			create_compute_shader(g_device, g_managed_resources.compute_shaders["post_dlss"_h].put(), L"PostDLSS_cs.hlsl");
		}

		// Create UAV.
		ensure(g_device->CreateUnorderedAccessView(g_managed_resources.resources["taa"_h].get(), nullptr, g_managed_resources.unordered_access_views["taa"_h].put()), >= 0);

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["taa"_h], nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["post_dlss"_h].get(), nullptr, 0);
		const std::array post_dlss_cbs = { g_managed_resources.buffers["taa_b1"_h].get(), g_managed_resources.buffers["taa_b2"_h].get() };
		ctx->CSSetConstantBuffers(1, pre_dlss_cbs.size(), pre_dlss_cbs.data());
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["ro_postfx_luminance_buffautoexposure"_h]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

		//
	}
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
	hr = ps->GetPrivateData(g_ps_ssao_0x94445D2D.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_0x94445D2D.hash) {
		// SRV0 should be depth.
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->PSGetShaderResources(0, 1, srv_depth.put());

		Com_ptr<ID3D11Buffer> cb_original;
		ctx->PSGetConstantBuffers(1, 1, cb_original.put());
		Com_ptr<ID3D11SamplerState> smp_original;
		ctx->PSGetSamplers(0, 1, smp_original.put());

		// VB-GTAOPrefilterDepths16x16 pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["vb_gtao_prefilter_depths16x16"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["vb_gtao_prefilter_depths16x16"_h].put(), L"VB-GTAO_impl.hlsl", "prefilter_depths16x16_cs", defines);
		}

		// Create sampler.
		[[unlikely]] if (!g_managed_resources.samplers["point_clamp"_h]) {
			create_sampler_point_clamp(g_device, g_managed_resources.samplers["point_clamp"_h].put());
		}

		// Create prefilter depths views.
		[[unlikely]] if (!g_uav_vb_gtao_prefilter_depths16x16[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = VB_GTAO_DEPTH_MIP_LEVELS;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			// Create UAVs for each MIP.
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = tex_desc.Format;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			for (int i = 0; i < g_uav_vb_gtao_prefilter_depths16x16.size(); ++i) {
			   uav_desc.Texture2D.MipSlice = i;
			   ensure(g_device->CreateUnorderedAccessView(tex.get(), &uav_desc, &g_uav_vb_gtao_prefilter_depths16x16[i]), >= 0);
			}

			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["vb_gtao_prefilter_depths16x16"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, g_uav_vb_gtao_prefilter_depths16x16.size(), g_uav_vb_gtao_prefilter_depths16x16.data(), nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["vb_gtao_prefilter_depths16x16"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &cb_original);
		ctx->CSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
		ctx->CSSetShaderResources(0, 1, &srv_depth);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, g_uav_vb_gtao_prefilter_depths16x16.size()> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//
		
		// VB-GTAOMainPass pass
		//
		// We will render to the original RT (rgba8_unorm).
		// Also we will skip denoise here and rely on the game's denoiser instead.
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(g_device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["vb_gtao_main_pass"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string vb_gtao_quality_val = std::to_string(g_vb_gtao_quality);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "VB_GTAO_QUALITY", vb_gtao_quality_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["vb_gtao_main_pass"_h].put(), L"VB-GTAO_impl.hlsl", "main_pass_ps", defines);
		}

		// Bindings.
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["vb_gtao_main_pass"_h].get(), nullptr, 0);
		ctx->PSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["vb_gtao_prefilter_depths16x16"_h]);

		ctx->Draw(3, 0);

		#if DEV && SHOW_AO
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource;
		rtv->GetResource(resource.put());
		ensure(g_device->CreateShaderResourceView(resource.get(), nullptr, g_managed_resources.shader_resource_views["ao"_h].put()), >= 0);
		#endif

		//

		// Restore.
		ctx->PSSetSamplers(0, 1, &smp_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_denoise_0x4CD2BE39.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_denoise_0x4CD2BE39.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["denoise_0x4CD2BE39"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["denoise_0x4CD2BE39"_h].put(), L"Denoise_0x4CD2BE39_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["denoise_0x4CD2BE39"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h]) {
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].put(), L"MotionBlurAndLensDistortionMVs_0xC97890C8_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xE908E905"_h]) {
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].put(), L"MotionBlurAndLensDistortionMVs_0xE908E905_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_downsample_0x42873B15.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_downsample_0x42873B15.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["downsample_0x42873B15"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["downsample_0x42873B15"_h].put(), L"Downsample_0x42873B15_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["downsample_0x42873B15"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xA6F33860.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xA6F33860.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0xA6F33860"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0xA6F33860"_h].put(), L"Tonemap_0xA6F33860_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0xA6F33860"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x11E16EF3.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x11E16EF3.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x11E16EF3"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0x11E16EF3"_h].put(), L"Tonemap_0x11E16EF3_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x11E16EF3"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_vignette_0xFD4216EA.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_vignette_0xFD4216EA.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["vignette_0xFD4216EA"_h]) {
			const std::string vignette_strenght_str = std::to_string(g_vignette_strenght);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGNETTE_STRENGHT", vignette_strenght_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["vignette_0xFD4216EA"_h].put(), L"Vignette_0xFD4216EA_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["vignette_0xFD4216EA"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_upsample_0x1A0CD2AE.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_upsample_0x1A0CD2AE.hash) {

		#if DEV && SHOW_AO
		if (g_managed_resources.shader_resource_views["ao"_h]) {
			ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["ao"_h]);
		}
		#endif

		// Backup RTs.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);
		
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
			// SRV0 should be the scene.
			Com_ptr<ID3D11ShaderResourceView> srv;
			ctx->PSGetShaderResources(0, 1, srv.put());
			
			// Get texture description.
			Com_ptr<ID3D11Resource> resource;
			srv->GetResource(resource.put());
			Com_ptr<ID3D11Texture2D> tex;
			resource->QueryInterface(tex.put());
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["amd_ffx_cas"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["amd_ffx_cas"_h], nullptr);
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get(), nullptr, 0);

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
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D_SHADER_MACRO defines[] = {
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
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
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["amd_ffx_cas"_h]);

		ctx->Draw(3, 0);

		//

		return true;
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
	hr = cs->GetPrivateData(g_cs_downsample_depth_0x27BD5265.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_downsample_depth_0x27BD5265.hash) {
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->CSGetShaderResources(0, 1, srv_depth.put());
		srv_depth->GetResource(g_managed_resources.resources["depth"_h].put());
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_taa_0x06BBC941.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0x06BBC941.hash) {
		if (g_enable_dlss) {
			// We expect deferred context here.
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);

			g_taa_tag = (uintptr_t)ctx;

			ctx->CSGetConstantBuffers(1, 1, g_managed_resources.buffers["taa_b1"_h].put());
			ctx->CSGetConstantBuffers(2, 1, g_managed_resources.buffers["taa_b2"_h].put());

			// SRV1 should be motion vectors.
			// SRV2 should be the scene.
			// SRV3 should be the buffer ro_postfx_luminance_buffautoexposure.
			std::array<ID3D11ShaderResourceView*, 3> srvs = {};
			ctx->CSGetShaderResources(1, srvs.size(), srvs.data());
			srvs[0]->GetResource(g_managed_resources.resources["mvs"_h].put());
			srvs[1]->GetResource(g_managed_resources.resources["scene"_h].put());
			g_managed_resources.shader_resource_views["ro_postfx_luminance_buffautoexposure"_h] = srvs[2];

			// UAV1 should be the TAA out.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(1, 1, uav.put());
			uav->GetResource(g_managed_resources.resources["taa"_h].put());

			release_com_array(srvs);
			return true;
		}

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["TAA_0x06BBC941"_h]) {
			create_compute_shader(g_device, g_managed_resources.compute_shaders["TAA_0x06BBC941"_h].put(), L"TAA_0x06BBC941_cs.hlsl");
		}

		// Bindings.
		ctx->CSSetShader(g_managed_resources.compute_shaders["TAA_0x06BBC941"_h].get(), nullptr, 0);

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
				case g_ps_ssao_0x94445D2D.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_0x94445D2D.guid, sizeof(g_ps_ssao_0x94445D2D.hash), &g_ps_ssao_0x94445D2D.hash), >= 0);
					return;
				case g_ps_tonemap_0xA6F33860.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xA6F33860.guid, sizeof(g_ps_tonemap_0xA6F33860.hash), &g_ps_tonemap_0xA6F33860.hash), >= 0);
					return;
				case g_ps_tonemap_0x11E16EF3.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x11E16EF3.guid, sizeof(g_ps_tonemap_0x11E16EF3.hash), &g_ps_tonemap_0x11E16EF3.hash), >= 0);
					return;
				case g_ps_vignette_0xFD4216EA.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_vignette_0xFD4216EA.guid, sizeof(g_ps_vignette_0xFD4216EA.hash), &g_ps_vignette_0xFD4216EA.hash), >= 0);
					return;
				case g_ps_upsample_0x1A0CD2AE.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_upsample_0x1A0CD2AE.guid, sizeof(g_ps_upsample_0x1A0CD2AE.hash), &g_ps_upsample_0x1A0CD2AE.hash), >= 0);
					return;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash), >= 0);
					return;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash), >= 0);
					return;
				case g_ps_denoise_0x4CD2BE39.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_denoise_0x4CD2BE39.guid, sizeof(g_ps_denoise_0x4CD2BE39.hash), &g_ps_denoise_0x4CD2BE39.hash), >= 0);
					return;
				case g_ps_downsample_0x42873B15.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_downsample_0x42873B15.guid, sizeof(g_ps_downsample_0x42873B15.hash), &g_ps_downsample_0x42873B15.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_downsample_depth_0x27BD5265.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_downsample_depth_0x27BD5265.guid, sizeof(g_cs_downsample_depth_0x27BD5265.hash), &g_cs_downsample_depth_0x27BD5265.hash), >= 0);
					return;
				case g_cs_taa_0x06BBC941.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0x06BBC941.guid, sizeof(g_cs_taa_0x06BBC941.hash), &g_cs_taa_0x06BBC941.hash), >= 0);
					return;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_managed_resources.buffers["taa_b1"_h]) {
		g_taa_cb1_mapped_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_managed_resources.buffers["taa_b1"_h]) {
		auto data = (PerViewCB*)g_taa_cb1_mapped_data;
		g_jitter_x = data->cb_projectionmatrix.m20;
		g_jitter_y = data->cb_projectionmatrix.m21;
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		// Backbuffer.
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_typeless) {
			
			#if  DEV
			// We only expect r8g8b8a8_unorm_srgb.
			if (desc.format == reshade::api::format::r8g8b8a8_unorm) {
				log_debug("The game requested r8g8b8a8_unorm view!");
			}
			#endif

			if (desc.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
				desc.format = reshade::api::format::r16g16b16a16_unorm;
				return true;
			}
			return false;
		}
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
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTVs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}

		// After tonemap all RTs should be r8g8b8a8_typeless.
		// We only expect r8g8b8a8_unorm_srgb views.
		assert(g_swapchain_width && g_swapchain_height);
		if (desc.texture.format == reshade::api::format::r8g8b8a8_typeless && desc.texture.width == g_swapchain_width && desc.texture.height == g_swapchain_height) {
			desc.texture.format = reshade::api::format::r16g16b16a16_typeless;
			return true;
		}

		// Crashing the game on settings change or save game overwrite.
		//if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		//	if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
		//		desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
		//		return true;
		//	}
		//}
	}
	else if ((desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
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
		// Game usese 4 and 16.
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

	// Reset resolution dependent resources.
	g_managed_resources.compute_shaders["vb_gtao_prefilter_depths16x16"_h].reset();
	release_com_array(g_uav_vb_gtao_prefilter_depths16x16);
	g_managed_resources.pixel_shaders["vb_gtao_main_pass"_h].reset();
}

static void on_destroy_device(reshade::api::device* device)
{
	if (device->get_native() != (uintptr_t)g_device) {
		return;
	}
	if (g_enable_dlss) {
		DLSS::get_instance().shutdown();
	}
	release_com_array(g_uav_vb_gtao_prefilter_depths16x16);
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

	if (!reshade::get_config_value(nullptr, NAME, "DisableLensDistortion", g_disable_lens_distortion)) {
		reshade::set_config_value(nullptr, NAME, "DisableLensDistortion", g_disable_lens_distortion);
	}
	if (!reshade::get_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt);
	}
	if (!reshade::get_config_value(nullptr, NAME, "VignetteStrenght", g_vignette_strenght)) {
		reshade::set_config_value(nullptr, NAME, "VignetteStrenght", g_vignette_strenght);
	}
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, NAME, "VBGTAOQuality", g_vb_gtao_quality)) {
		reshade::set_config_value(nullptr, NAME, "VBGTAOQuality", g_vb_gtao_quality);
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

	if (ImGui::Checkbox("Disable lens distortion", &g_disable_lens_distortion)) {
		g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].reset();
		g_managed_resources.pixel_shaders["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].reset();
		reshade::set_config_value(nullptr, NAME, "DisableLensDistortion", g_disable_lens_distortion);
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens dirt", &g_disable_lens_dirt)) {
		g_managed_resources.pixel_shaders["tonemap_0xA6F33860"_h].reset();
		g_managed_resources.pixel_shaders["tonemap_0x11E16EF3"_h].reset();
		reshade::set_config_value(nullptr, NAME, "DisableLensDirt", g_disable_lens_dirt);
	}
	ImGui::Spacing();

	static constexpr std::array vb_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("VB-GTAO quality", &g_vb_gtao_quality, vb_gtao_quality_items.data(), vb_gtao_quality_items.size())) {
		g_managed_resources.pixel_shaders["vb_gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "VBGTAOQuality", g_vb_gtao_quality);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["tonemap_0xA6F33860"_h].reset();
		g_managed_resources.pixel_shaders["tonemap_0x11E16EF3"_h].reset();
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Vignette strenght", &g_vignette_strenght, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["vignette_0xFD4216EA"_h].reset();
		reshade::set_config_value(nullptr, NAME, "VignetteStrenght", g_vignette_strenght);
	}
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
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "TRC", g_trc);
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Gamma", g_gamma);
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
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
			reshade::register_event<reshade::addon_event::execute_secondary_command_list>(on_execute_secondary_command_list);
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
