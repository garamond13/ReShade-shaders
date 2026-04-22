#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS.h"
#include "BlueNoiseTex.h"

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

constexpr Shader_hash g_ps_ssao_0x94445D2D = { 0x94445D2D, { 0x26f02327, 0xa1ee, 0x4d2a, { 0x9a, 0xdd, 0xbb, 0x33, 0xfe, 0x1e, 0x5d, 0x71 }}};
constexpr Shader_hash g_ps_denoise_0x4CD2BE39 = { 0x4CD2BE39, { 0xe72ed430, 0x45eb, 0x46ce, { 0xa4, 0xa1, 0x56, 0x8b, 0xb1, 0xa3, 0x90, 0x85 }}};
constexpr Shader_hash g_cs_downsample_depth_0x27BD5265 = { 0x27BD5265, { 0x2dc964d7, 0x62f6, 0x4267, { 0xa0, 0x2, 0xc8, 0x77, 0x51, 0xc2, 0x56, 0xab }}};
constexpr Shader_hash g_cs_taa_0x9F77B624 = { 0x9F77B624, { 0x867178ce, 0xcca3, 0x4d5e, { 0xb6, 0x27, 0x9e, 0x6d, 0xe3, 0x4c, 0x95, 0x88 }}};

// If motion blur is enabled in the game's settings.
constexpr Shader_hash g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8 = { 0xC97890C8, { 0x51783160, 0x2604, 0x4d2e, { 0x91, 0xf2, 0x68, 0x35, 0x58, 0x7a, 0xf6, 0xb1 }}};

// If motion blur is disabled in the game's settings.
constexpr Shader_hash g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905 = { 0xE908E905, { 0x337d2791, 0xda14, 0x48f1, { 0x85, 0x2d, 0x5b, 0xcc, 0x3, 0x7e, 0xfa, 0x60 }}};

// Black and white.
constexpr Shader_hash g_ps_tonemap_0x606F9A37 = { 0x606F9A37, { 0xe387339c, 0x6afe, 0x440e, { 0xbd, 0x53, 0x87, 0x40, 0xdb, 0x5c, 0xfc, 0x30 }}};

constexpr Shader_hash g_ps_tonemap_0xD2F50617 = { 0xD2F50617, { 0x13c01144, 0x4b23, 0x47af, { 0x93, 0x7f, 0x88, 0x6d, 0xe0, 0x16, 0x49, 0xa5 }}};
constexpr Shader_hash g_ps_vignette_0xFD4216EA = { 0xFD4216EA, { 0x259221d7, 0xe098, 0x4ee0, { 0x9a, 0xa6, 0x8a, 0x2e, 0x41, 0x10, 0x31, 0x2f }}};
constexpr Shader_hash g_ps_upsample_0x1A0CD2AE = { 0x1A0CD2AE, { 0x37c96d54, 0xd0a4, 0x44a8, { 0xb9, 0x54, 0x55, 0xff, 0xde, 0x53, 0xda, 0x4 }}};

//

static Graphical_upgrade_cb g_graphical_upgrade_cb;
static PerViewCB g_PerViewCB_data;
static void* g_PerViewCB_mapped_data;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_disable_lens_dirt = true;
static float g_vignette_strenght = 1.0f;
static float g_bloom_intensity = 1.0f;
static bool g_disable_lens_distortion;
static float g_amd_ffx_cas_sharpness = 0.4f;
static float g_amd_ffx_lfga_amount = 0.35f;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static float g_xe_gtao_radius = 0.6f;
static int g_xe_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

// DLSS.
static std::atomic<uintptr_t> g_taa_tag;
static bool g_enable_dlss;
static DLSS_PRESET g_dlss_preset = DLSS_PRESET_F;
static uintptr_t g_dlss_device;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// Device resources.
static std::array<ID3D11UnorderedAccessView*, XE_GTAO_DEPTH_MIP_LEVELS> g_uav_xe_gtao_prefilter_depths16x16;

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
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// PreDLSS pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["pre_dlss"_h]) {
			create_compute_shader(device.get(), g_cs["pre_dlss"_h].put(), L"PreDLSS_cs.hlsl");
		}

		// Create UAV.
		[[unlikely]] if (!g_uav["scene"_h]) {
			ensure(device->CreateUnorderedAccessView(g_resource["scene"_h].get(), nullptr, g_uav["scene"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav["scene"_h], nullptr);
		ctx->CSSetShader(g_cs["pre_dlss"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &g_cb["taa_b1"_h]);
		ctx->CSSetConstantBuffers(2, 1, &g_cb["taa_b2"_h]);
		ctx->CSSetShaderResources(0, 1, &g_srv["ro_postfx_luminance_buffautoexposure"_h]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

		// Unbind.
		// This doesn't seem to be necessary, but the game has random issues.
		static constexpr ID3D11UnorderedAccessView* uav_null = {};
		static constexpr ID3D11ShaderResourceView* srv_null = {};
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);
		ctx->CSSetShaderResources(0, 1, &srv_null);
		
		//

		// DLSS pass
		// 

		// These need to be valid.
		assert(g_resource["scene"_h]);
		assert(g_resource["depth"_h]);
		assert(g_resource["mvs"_h]);
		assert(g_resource["taa"_h]);

		NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
		eval_params.Feature.pInColor = g_resource["scene"_h].get();
		eval_params.Feature.pInOutput = g_resource["taa"_h].get();
		eval_params.pInDepth = g_resource["depth"_h].get();
		eval_params.pInMotionVectors = g_resource["mvs"_h].get();

		// MVs are in UV space so we need to scale them to screen space for DLSS.
		// Also for DLSS we need to flip the sign for both x and y.
		eval_params.InMVScaleX = -g_swapchain_width;
		eval_params.InMVScaleY = -g_swapchain_height;

		eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
		eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

		// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
		eval_params.InJitterOffsetX = g_PerViewCB_data.cb_projectionmatrix.m20 * (float)g_swapchain_width * -0.5f;
		eval_params.InJitterOffsetY = g_PerViewCB_data.cb_projectionmatrix.m21 * (float)g_swapchain_height * 0.5f;

		DLSS::instance().draw(ctx.get(), eval_params);

		//

		// PostDLSS pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["post_dlss"_h]) {
			create_compute_shader(device.get(), g_cs["post_dlss"_h].put(), L"PostDLSS_cs.hlsl");
		}

		// Create UAV.
		[[unlikely]] if (!g_uav["taa"_h]) {
			ensure(device->CreateUnorderedAccessView(g_resource["taa"_h].get(), nullptr, g_uav["taa"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav["taa"_h], nullptr);
		ctx->CSSetShader(g_cs["post_dlss"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &g_cb["taa_b1"_h]);
		ctx->CSSetConstantBuffers(2, 1, &g_cb["taa_b2"_h]);
		ctx->CSSetShaderResources(0, 1, &g_srv["ro_postfx_luminance_buffautoexposure"_h]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

		// Unbind.
		// This doesn't seem to be necessary, but the game has random issues.
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);
		ctx->CSSetShaderResources(0, 1, &srv_null);

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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_ssao_0x94445D2D.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_0x94445D2D.hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// SRV0 should be depth.
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->PSGetShaderResources(0, 1, srv_depth.put());

		Com_ptr<ID3D11Buffer> cb_original;
		ctx->PSGetConstantBuffers(1, 1, cb_original.put());
		Com_ptr<ID3D11SamplerState> smp_original;
		ctx->PSGetSamplers(0, 1, smp_original.put());

		// XeGTAOPrefilterDepths16x16 pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["xe_gtao_prefilter_depths16x16"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string effect_radius_str = std::to_string(g_xe_gtao_radius);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "EFFECT_RADIUS", effect_radius_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(device.get(), g_cs["xe_gtao_prefilter_depths16x16"_h].put(), L"XeGTAO.hlsl", "prefilter_depths16x16_cs", defines);
		}

		// Create sampler.
		[[unlikely]] if (!g_smp["point_clamp"_h]) {
			create_sampler_point_clamp(device.get(), g_smp["point_clamp"_h].put());
		}

		// Create prefilter depths views.
		[[unlikely]] if (!g_uav_xe_gtao_prefilter_depths16x16[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = XE_GTAO_DEPTH_MIP_LEVELS;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			// Create UAVs for each MIP.
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = tex_desc.Format;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			for (int i = 0; i < g_uav_xe_gtao_prefilter_depths16x16.size(); ++i) {
			   uav_desc.Texture2D.MipSlice = i;
			   ensure(device->CreateUnorderedAccessView(tex.get(), &uav_desc, &g_uav_xe_gtao_prefilter_depths16x16[i]), >= 0);
			}

			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["xe_gtao_prefilter_depths16x16"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, g_uav_xe_gtao_prefilter_depths16x16.size(), g_uav_xe_gtao_prefilter_depths16x16.data(), nullptr);
		ctx->CSSetShader(g_cs["xe_gtao_prefilter_depths16x16"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &cb_original);
		ctx->CSSetSamplers(0, 1, &g_smp["point_clamp"_h]);
		ctx->CSSetShaderResources(0, 1, &srv_depth);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, g_uav_xe_gtao_prefilter_depths16x16.size()> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//
		
		// XeGTAOMainPass pass
		//
		// We will render to the original RT (rgba8_unorm).
		// Also we will skip denoise here and rely on the game's denoiser instead.
		//

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["xe_gtao_main_pass"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string xe_gtao_quality_val = std::to_string(g_xe_gtao_quality);
			const std::string effect_radius_str = std::to_string(g_xe_gtao_radius);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "XE_GTAO_QUALITY", xe_gtao_quality_val.c_str() },
				{ "EFFECT_RADIUS", effect_radius_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["xe_gtao_main_pass"_h].put(), L"XeGTAO.hlsl", "main_pass_ps", defines);
		}

		// Bindings.
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["xe_gtao_main_pass"_h].get(), nullptr, 0);
		ctx->PSSetSamplers(0, 1, &g_smp["point_clamp"_h]);
		ctx->PSSetShaderResources(0, 1, &g_srv["xe_gtao_prefilter_depths16x16"_h]);

		ctx->Draw(3, 0);

		#if DEV && SHOW_AO
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource;
		rtv->GetResource(resource.put());
		ensure(device->CreateShaderResourceView(resource.get(), nullptr, g_srv["ao"_h].put()), >= 0);
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
		[[unlikely]] if (!g_ps["denoise_0x4CD2BE39"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			create_pixel_shader(device.get(), g_ps["denoise_0x4CD2BE39"_h].put(), L"Denoise_0x4CD2BE39_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_ps["denoise_0x4CD2BE39"_h].get(), nullptr, 0);
		
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash) {
		// Create PS.
		[[unlikely]] if (!g_ps["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].put(), L"MotionBlurAndLensDistortionMVs_0xC97890C8_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].get(), nullptr, 0);
		
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash) {
		// Create PS.
		[[unlikely]] if (!g_ps["motion_blur_and_lens_distortion_mvs_0xE908E905"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].put(), L"MotionBlurAndLensDistortionMVs_0xE908E905_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].get(), nullptr, 0);
		
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x606F9A37.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x606F9A37.hash) {
		// Create PS.
		[[unlikely]] if (!g_ps["tonemap_0x606F9A37"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["tonemap_0x606F9A37"_h].put(), L"Tonemap_0x606F9A37_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["tonemap_0x606F9A37"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xD2F50617.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xD2F50617.hash) {
		// Create PS.
		[[unlikely]] if (!g_ps["tonemap_0xD2F50617"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["tonemap_0xD2F50617"_h].put(), L"Tonemap_0xD2F50617_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["tonemap_0xD2F50617"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_vignette_0xFD4216EA.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_vignette_0xFD4216EA.hash) {
		// Create PS.
		[[unlikely]] if (!g_ps["vignette_0xFD4216EA"_h]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string vignette_strenght_str = std::to_string(g_vignette_strenght);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGNETTE_STRENGHT", vignette_strenght_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["vignette_0xFD4216EA"_h].put(), L"Vignette_0xFD4216EA_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["vignette_0xFD4216EA"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_upsample_0x1A0CD2AE.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_upsample_0x1A0CD2AE.hash) {

		#if DEV && SHOW_AO
		if (g_srv["ao"_h]) {
			ctx->PSSetShaderResources(0, 1, &g_srv["ao"_h]);
		}
		#endif
		
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Backup RTs.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);
		
		// AMD FFX CAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["amd_ffx_cas"_h]) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["amd_ffx_cas"_h].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["amd_ffx_cas"_h]) {
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
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["amd_ffx_cas"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["amd_ffx_cas"_h], nullptr);
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["amd_ffx_cas"_h].get(), nullptr, 0);

		ctx->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

		[[unlikely]] if (!g_cb["cb"_h]) {
			create_constant_buffer(device.get(), sizeof(g_graphical_upgrade_cb), g_cb["cb"_h].put());
		}

		// Create PS.
		[[unlikely]] if (!g_ps["amd_ffx_lfga"_h]) {
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D_SHADER_MACRO defines[] = {
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["amd_ffx_lfga"_h].put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
		}

		// Create blue noise texture.
		[[unlikely]] if (!g_srv["blue_noise_tex"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = BLUE_NOISE_TEX_WIDTH;
			tex_desc.Height = BLUE_NOISE_TEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = BLUE_NOISE_TEX_ARRAY_SIZE;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			std::array<D3D11_SUBRESOURCE_DATA, BLUE_NOISE_TEX_ARRAY_SIZE> subresource_data;
			for (size_t i = 0; i < subresource_data.size(); ++i) {
				subresource_data[i].pSysMem = BLUE_NOISE_TEX + i * BLUE_NOISE_TEX_PITCH * BLUE_NOISE_TEX_HEIGHT;
				subresource_data[i].SysMemPitch = BLUE_NOISE_TEX_PITCH;
			}
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, subresource_data.data(), tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["blue_noise_tex"_h].put()), >= 0);
		}

		// Update the constant buffer.
		// We need to limit the temporal grain update rate, otherwise grain will flicker.
		constexpr auto interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / (double)BLUE_NOISE_TEX_ARRAY_SIZE));
		static auto last_update = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		if (now - last_update >= interval) {
			g_graphical_upgrade_cb.tex_noise_index += 1;
			if (g_graphical_upgrade_cb.tex_noise_index >= BLUE_NOISE_TEX_ARRAY_SIZE) {
				g_graphical_upgrade_cb.tex_noise_index = 0;
			}
			update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_graphical_upgrade_cb, sizeof(g_graphical_upgrade_cb));
			last_update += interval;
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_ps["amd_ffx_lfga"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb["cb"_h]);
		const std::array ps_srvs_amd_ffx_lfga = { g_srv["amd_ffx_cas"_h].get(), g_srv["blue_noise_tex"_h].get() };
		ctx->PSSetShaderResources(0, ps_srvs_amd_ffx_lfga.size(), ps_srvs_amd_ffx_lfga.data());

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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = cs->GetPrivateData(g_cs_downsample_depth_0x27BD5265.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_downsample_depth_0x27BD5265.hash) {
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->CSGetShaderResources(0, 1, srv_depth.put());
		srv_depth->GetResource(g_resource["depth"_h].put());
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_taa_0x9F77B624.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0x9F77B624.hash) {
		if (g_enable_dlss) {
			// We expect deferred context here.
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);

			g_taa_tag = (uintptr_t)ctx;

			// Get CBs.
			ctx->CSGetConstantBuffers(1, 1, g_cb["taa_b1"_h].put());
			ctx->CSGetConstantBuffers(2, 1, g_cb["taa_b2"_h].put());

			// SRV1 should be motion vectors.
			// SRV2 should be the scene.
			// SRV3 should be the buffer ro_postfx_luminance_buffautoexposure.
			std::array<ID3D11ShaderResourceView*, 3> srvs = {};
			ctx->CSGetShaderResources(1, srvs.size(), srvs.data());
			srvs[0]->GetResource(g_resource["mvs"_h].put());
			srvs[1]->GetResource(g_resource["scene"_h].put());
			g_srv["ro_postfx_luminance_buffautoexposure"_h] = srvs[2];

			// UAV1 should be the TAA out.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(1, 1, uav.put());
			uav->GetResource(g_resource["taa"_h].put());

			release_com_array(srvs);
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
				case g_ps_ssao_0x94445D2D.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_0x94445D2D.guid, sizeof(g_ps_ssao_0x94445D2D.hash), &g_ps_ssao_0x94445D2D.hash), >= 0);
					return;
				case g_ps_denoise_0x4CD2BE39.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_denoise_0x4CD2BE39.guid, sizeof(g_ps_denoise_0x4CD2BE39.hash), &g_ps_denoise_0x4CD2BE39.hash), >= 0);
					return;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8.hash), >= 0);
					return;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905.hash), >= 0);
					return;
				case g_ps_tonemap_0x606F9A37.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x606F9A37.guid, sizeof(g_ps_tonemap_0x606F9A37.hash), &g_ps_tonemap_0x606F9A37.hash), >= 0);
					return;
				case g_ps_tonemap_0xD2F50617.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xD2F50617.guid, sizeof(g_ps_tonemap_0xD2F50617.hash), &g_ps_tonemap_0xD2F50617.hash), >= 0);
					return;
				case g_ps_vignette_0xFD4216EA.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_vignette_0xFD4216EA.guid, sizeof(g_ps_vignette_0xFD4216EA.hash), &g_ps_vignette_0xFD4216EA.hash), >= 0);
					return;
				case g_ps_upsample_0x1A0CD2AE.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_upsample_0x1A0CD2AE.guid, sizeof(g_ps_upsample_0x1A0CD2AE.hash), &g_ps_upsample_0x1A0CD2AE.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_taa_0x9F77B624.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0x9F77B624.guid, sizeof(g_cs_taa_0x9F77B624.hash), &g_cs_taa_0x9F77B624.hash), >= 0);
					return;
				case g_cs_downsample_depth_0x27BD5265.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_downsample_depth_0x27BD5265.guid, sizeof(g_cs_downsample_depth_0x27BD5265.hash), &g_cs_downsample_depth_0x27BD5265.hash), >= 0);
					return;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_cb[hash_name("taa_b1")]) {
		g_PerViewCB_mapped_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_cb[hash_name("taa_b1")]) {
		std::memcpy(&g_PerViewCB_data, g_PerViewCB_mapped_data, sizeof(g_PerViewCB_data));
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
	// Some upgrades are breaking the game randomly and for no obvious reason, they are left out.

	// Filter RTs.
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

		// Crashing the game on settings change or save game overwrite.
		//if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		//	if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
		//		desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
		//		return true;
		//	}
		//}

		// After tonemap all RTs should be r8g8b8a8_typeless.
		// We only expect r8g8b8a8_unorm_srgb views.
		assert(g_swapchain_width && g_swapchain_height);
		if (desc.texture.format == reshade::api::format::r8g8b8a8_typeless && desc.texture.width == g_swapchain_width && desc.texture.height == g_swapchain_height) {
			desc.texture.format = reshade::api::format::r16g16b16a16_typeless;
			return true;
		}
	}
	else if ((desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
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
		
		// The game usese 4 and 16.
		desc.max_anisotropy = 16.0f;
		
		// As recommended for DLAA.
		desc.mip_lod_bias += -1.0f;
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

	// Minimum supported resolution by DLSS.
	// The game may create the 2nd swapchain that should be 2x2. This may be caused by Epic Garbage?
	if (desc.BufferDesc.Width < 32 && desc.BufferDesc.Height < 32) {
		return;
	}

	// Set maximum frame latency to 1.
	Com_ptr<ID3D11Device> device;
	ensure(native_swapchain->GetDevice(IID_PPV_ARGS(device.put())), >= 0);
	Com_ptr<IDXGIDevice1> device1;
	auto hr = device->QueryInterface(device1.put());
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		device->GetImmediateContext(ctx.put());
		DLSS::instance().init(device.get());
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
		g_dlss_device = (uintptr_t)device.get();
	}

	// Reset resolution dependent resources.
	g_cs["xe_gtao_prefilter_depths16x16"_h].reset();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	g_ps["xe_gtao_main_pass"_h].reset();
	g_rtv["amd_ffx_cas"_h].reset();
}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	if (device->get_api() != reshade::api::device_api::d3d11) {
		return;
	}

	if (g_enable_dlss) {
		auto native_device = (ID3D11Device*)device->get_native();
		DLSS::instance().init(native_device);
	}
}

static void on_destroy_device(reshade::api::device *device)
{
	// The game may create additional devices and destroy them later, after the main device is created.
	if (g_enable_dlss && device->get_native() == g_dlss_device) {
		DLSS::instance().shutdown();
		g_dlss_device = 0;
	}

	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	clear_device_resources();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DLSSPreset", g_dlss_preset)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DLSSPreset", g_dlss_preset);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "VignetteStrenght", g_vignette_strenght)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "VignetteStrenght", g_vignette_strenght);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
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
		auto device = (ID3D11Device*)runtime->get_device()->get_native();
		Com_ptr<IDXGIDevice1> device1;
		ensure(device->QueryInterface(device1.put()), >= 0);
		UINT max_latency;
		ensure(device1->GetMaximumFrameLatency(&max_latency), >= 0);
		log_debug("MaximumFrameLatency: {}", max_latency);
	}
	ImGui::NewLine();
	#endif

	auto device = (ID3D11Device*)runtime->get_device()->get_native();
	Com_ptr<ID3D11DeviceContext> ctx;
	device->GetImmediateContext(ctx.put());

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			DLSS::instance().init(device);
			DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
			g_dlss_device = 0;
		}
		else {
			DLSS::instance().shutdown();
			g_dlss_device = 0;
		}
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "E", "F", "K" };
	if (ImGui::Combo("DLSS preset", &g_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DLSSPreset", g_dlss_preset);
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::SliderFloat("XeGTAO radius", &g_xe_gtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
		g_cs["xe_gtao_prefilter_depths16x16"_h].reset();
		g_ps["xe_gtao_main_pass"_h].reset();
	}
	static constexpr std::array xe_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO quality", &g_xe_gtao_quality, xe_gtao_quality_items.data(), xe_gtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
		g_ps["xe_gtao_main_pass"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens distortion", &g_disable_lens_distortion)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion);
		g_ps["motion_blur_and_lens_distortion_mvs_0xC97890C8"_h].reset();
		g_ps["motion_blur_and_lens_distortion_mvs_0xE908E905"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens dirt", &g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt);
		g_ps["tonemap_0x606F9A37"_h].reset();
		g_ps["tonemap_0xD2F50617"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps["tonemap_0x606F9A37"_h].reset();
		g_ps["tonemap_0xD2F50617"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Vignette strenght", &g_vignette_strenght, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "VignetteStrenght", g_vignette_strenght);
		g_ps["vignette_0xFD4216EA"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps["amd_ffx_cas"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "TRC", g_trc);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "Gamma", g_gamma);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DishonoredDOTOGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();
}

extern "C" __declspec(dllexport) const char* NAME = "DishonoredDOTOGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "DishonoredDOTOGraphicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/DishonoredDOTOGraphicalUpgrade";

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