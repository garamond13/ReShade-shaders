#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Helpers.h"
#include "TRC.h"
#include "DLSS.h"
#include "HLSLTypes.h"
#include "BlueNoiseTex.h"

struct alignas(16) CB_data
{
   float tex_noise_index;
};

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
	float4 cb_jittervectors; // xy current and zw previous, in uv offsets.
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

constexpr uint32_t g_ps_ssao_0x94445D2D_hash = 0x94445D2D;
constexpr GUID g_ps_ssao_0x94445D2D_guid = { 0x37961341, 0x530f, 0x405b, { 0xa7, 0x38, 0xdf, 0xc2, 0x5c, 0xaa, 0x32, 0xdc } };

constexpr uint32_t g_ps_tonemap_0xA6F33860_hash = 0xA6F33860;
constexpr GUID g_ps_tonemap_0xA6F33860_guid = { 0xdd2785dc, 0x1465, 0x418a, { 0xa9, 0xaf, 0xe2, 0x4f, 0x1e, 0xd0, 0x4c, 0x29 } };

constexpr uint32_t g_ps_tonemap_0x11E16EF3_hash = 0x11E16EF3;
constexpr GUID g_ps_tonemap_0x11E16EF3_guid = { 0xd0922fff, 0x1053, 0x46f8, { 0xa0, 0x91, 0x11, 0xd9, 0xa3, 0x4a, 0xdd, 0x89 } };

constexpr uint32_t g_ps_vignette_0xFD4216EA_hash = 0xFD4216EA;
constexpr GUID g_ps_vignette_0xFD4216EA_guid = { 0x96c298b8, 0xd56a, 0x4e67, { 0x90, 0x12, 0x8, 0x8a, 0xeb, 0x99, 0xb1, 0xf } };

constexpr uint32_t g_ps_upsample_0x1A0CD2AE_hash = 0x1A0CD2AE;
constexpr GUID g_ps_upsample_0x1A0CD2AE_guid = { 0xaaf2a4bb, 0x496b, 0x47be, { 0xbf, 0xf6, 0xae, 0x48, 0xb3, 0x3f, 0x6f, 0x50 } };

constexpr uint32_t g_cs_downsample_depth_0x27BD5265_hash = 0x27BD5265;
constexpr GUID g_cs_downsample_depth_0x27BD5265_guid = { 0xd2d58efa, 0x76e4, 0x45a6, { 0xae, 0x81, 0x46, 0x83, 0xd0, 0x11, 0x86, 0x28 } };

constexpr uint32_t g_cs_taa_0x06BBC941_hash = 0x06BBC941;
constexpr GUID g_cs_taa_0x06BBC941_guid = { 0x16cb60d, 0x938, 0x4e9d, { 0xa7, 0x7f, 0xea, 0x40, 0x56, 0x19, 0x9, 0xd6 } };

// If motion blur is enabled in the game's settings.
constexpr uint32_t g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_hash = 0xC97890C8;
constexpr GUID g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_guid = { 0x51783160, 0x2604, 0x4d2e, { 0x91, 0xf2, 0x68, 0x35, 0x58, 0x7a, 0xf6, 0xb1 } };

// If motion blur is disabled in the game's settings.
constexpr uint32_t g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_hash = 0xE908E905;
constexpr GUID g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_guid = { 0x337d2791, 0xda14, 0x48f1, { 0x85, 0x2d, 0x5b, 0xcc, 0x3, 0x7e, 0xfa, 0x60 } };

//

static CB_data g_cb_data;
static PerViewCB g_per_view_cb;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_disable_lens_dirt = true;
static float g_vignette_strenght = 1.0f;
static bool g_force_vsync_off = true;
static float g_bloom_intensity = 1.0f;
static bool g_disable_lens_distortion;
static float g_amd_ffx_cas_sharpness = 0.4f;
static float g_amd_ffx_lfga_amount = 0.4f;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static float g_xe_gtao_radius = 0.6f;
static int g_xe_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

// DLSS
static std::atomic<uintptr_t> g_taa_tag;
static void* g_taa_cb1_mapped_data;
static bool g_enable_dlss = true;
static DLSS_PRESET g_dlss_preset = DLSS_PRESET_F;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// COM resources.
static std::unordered_map<uint32_t, Com_ptr<ID3D11RenderTargetView>> g_rtv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11UnorderedAccessView>> g_uav;
static std::unordered_map<uint32_t, Com_ptr<ID3D11ShaderResourceView>> g_srv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11VertexShader>> g_vs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11PixelShader>> g_ps;
static std::unordered_map<uint32_t, Com_ptr<ID3D11ComputeShader>> g_cs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11SamplerState>> g_smp;
static std::unordered_map<uint32_t, Com_ptr<ID3D11Resource>> g_resource;
static std::unordered_map<uint32_t, Com_ptr<ID3D11Buffer>> g_cb;
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
		[[unlikely]] if (!g_cs[hash_name("pre_dlss")]) {
			create_compute_shader(device.get(), g_cs[hash_name("pre_dlss")].put(), L"PreDLSS_cs.hlsl");
		}

		// Create UAV.
		[[unlikely]] if (!g_uav[hash_name("scene")]) {
			ensure(device->CreateUnorderedAccessView(g_resource[hash_name("scene")].get(), nullptr, g_uav[hash_name("scene")].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav[hash_name("scene")], nullptr);
		ctx->CSSetShader(g_cs[hash_name("pre_dlss")].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &g_cb[hash_name("taa_b1")]);
		ctx->CSSetConstantBuffers(2, 1, &g_cb[hash_name("taa_b2")]);
		ctx->CSSetShaderResources(0, 1, &g_srv[hash_name("ro_postfx_luminance_buffautoexposure")]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);
		
		//

		// DLSS pass
		// 

		// These need to be valid.
		assert(g_resource[hash_name("scene")]);
		assert(g_resource[hash_name("depth")]);
		assert(g_resource[hash_name("mvs")]);
		assert(g_resource[hash_name("taa")]);

		NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
		eval_params.Feature.pInColor = g_resource[hash_name("scene")].get();
		eval_params.Feature.pInOutput = g_resource[hash_name("taa")].get();
		eval_params.pInDepth = g_resource[hash_name("depth")].get();
		eval_params.pInMotionVectors = g_resource[hash_name("mvs")].get();

		// MVs are in UV space so we need to scale them to screen space for DLSS.
		// Also for DLSS we need to flip the sign for both x and y.
		eval_params.InMVScaleX = -(float)g_swapchain_width;
		eval_params.InMVScaleY = -(float)g_swapchain_height;

		eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
		eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

		// We need to swap jitters. They are originally swapped or its just DLSS thing?
		// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
		eval_params.InJitterOffsetX = g_per_view_cb.cb_jittervectors.y * (float)g_swapchain_height;
		eval_params.InJitterOffsetY = g_per_view_cb.cb_jittervectors.x * (float)g_swapchain_width;

		DLSS::instance().draw(ctx.get(), eval_params);

		//

		// PostDLSS pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs[hash_name("post_dlss")]) {
			create_compute_shader(device.get(), g_cs[hash_name("post_dlss")].put(), L"PostDLSS_cs.hlsl");
		}

		// Create UAV.
		[[unlikely]] if (!g_uav[hash_name("taa")]) {
			ensure(device->CreateUnorderedAccessView(g_resource[hash_name("taa")].get(), nullptr, g_uav[hash_name("taa")].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav[hash_name("taa")], nullptr);
		ctx->CSSetShader(g_cs[hash_name("post_dlss")].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &g_cb[hash_name("taa_b1")]);
		ctx->CSSetConstantBuffers(2, 1, &g_cb[hash_name("taa_b2")]);
		ctx->CSSetShaderResources(0, 1, &g_srv[hash_name("ro_postfx_luminance_buffautoexposure")]);

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

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_ssao_0x94445D2D_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_0x94445D2D_hash) {
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
		[[unlikely]] if (!g_cs[hash_name("xe_gtao_prefilter_depths16x16")]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string effect_radius_str = std::to_string(g_xe_gtao_radius);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "EFFECT_RADIUS", effect_radius_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(device.get(), g_cs[hash_name("xe_gtao_prefilter_depths16x16")].put(), L"XeGTAO.hlsl", "prefilter_depths16x16_cs", defines);
		}

		// Create sampler.
		[[unlikely]] if (!g_smp[hash_name("point_clamp")]) {
			create_sampler_point_clamp(device.get(), g_smp[hash_name("point_clamp")].put());
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

			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv[hash_name("xe_gtao_prefilter_depths16x16")].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, g_uav_xe_gtao_prefilter_depths16x16.size(), g_uav_xe_gtao_prefilter_depths16x16.data(), nullptr);
		ctx->CSSetShader(g_cs[hash_name("xe_gtao_prefilter_depths16x16")].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(1, 1, &cb_original);
		ctx->CSSetSamplers(0, 1, &g_smp[hash_name("point_clamp")]);
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
		[[unlikely]] if (!g_vs[hash_name("fullscreen_triangle")]) {
			create_vertex_shader(device.get(), g_vs[hash_name("fullscreen_triangle")].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("xe_gtao_main_pass")]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string xe_gtao_quality_val = std::to_string(g_xe_gtao_quality);
			const std::string effect_radius_str = std::to_string(g_xe_gtao_radius);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "XE_GTAO_QUALITY", xe_gtao_quality_val.c_str() },
				{ "EFFECT_RADIUS", effect_radius_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("xe_gtao_main_pass")].put(), L"XeGTAO.hlsl", "main_pass_ps", defines);
		}

		// Bindings.
		ctx->VSSetShader(g_vs[hash_name("fullscreen_triangle")].get(), nullptr, 0);
		ctx->PSSetShader(g_ps[hash_name("xe_gtao_main_pass")].get(), nullptr, 0);
		ctx->PSSetSamplers(0, 1, &g_smp[hash_name("point_clamp")]);
		ctx->PSSetShaderResources(0, 1, &g_srv[hash_name("xe_gtao_prefilter_depths16x16")]);

		ctx->Draw(3, 0);

		#if DEV && SHOW_AO
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource;
		rtv->GetResource(resource.put());
		ensure(device->CreateShaderResourceView(resource.get(), nullptr, g_srv[hash_name("ao")].put()), >= 0);
		#endif

		//

		// Restore.
		ctx->PSSetSamplers(0, 1, &smp_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_hash) {
		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xC97890C8")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xC97890C8")].put(), L"MotionBlurAndLensDistortionMVs_0xC97890C8_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xC97890C8")].get(), nullptr, 0);
		
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_hash) {
		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xE908E905")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string disable_lens_distortion_str = std::to_string((int)g_disable_lens_distortion);
			const D3D_SHADER_MACRO defines[] = {
				{ "DISABLE_LENS_DISTORTION", disable_lens_distortion_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xE908E905")].put(), L"MotionBlurAndLensDistortionMVs_0xE908E905_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xE908E905")].get(), nullptr, 0);
		
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xA6F33860_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xA6F33860_hash) {
		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("tonemap_0xA6F33860")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("tonemap_0xA6F33860")].put(), L"Tonemap_0xA6F33860_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("tonemap_0xA6F33860")].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x11E16EF3_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x11E16EF3_hash) {
		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("tonemap_0x11E16EF3")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_dirt_str = std::to_string((int)g_disable_lens_dirt);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_DIRT", disable_lens_dirt_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("tonemap_0x11E16EF3")].put(), L"Tonemap_0x11E16EF3_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("tonemap_0x11E16EF3")].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_vignette_0xFD4216EA_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_vignette_0xFD4216EA_hash) {
		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("vignette")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string vignette_strenght_str = std::to_string(g_vignette_strenght);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGNETTE_STRENGHT", vignette_strenght_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("vignette")].put(), L"Vignette_0xFD4216EA_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_ps[hash_name("vignette")].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_upsample_0x1A0CD2AE_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_upsample_0x1A0CD2AE_hash) {

		#if DEV && SHOW_AO
		if (g_srv[hash_name("ao")]) {
			ctx->PSSetShaderResources(0, 1, &g_srv[hash_name("ao")]);
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
		[[unlikely]] if (!g_vs[hash_name("fullscreen_triangle")]) {
			create_vertex_shader(device.get(), g_vs[hash_name("fullscreen_triangle")].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("amd_ffx_cas")]) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D10_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("amd_ffx_cas")].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv[hash_name("amd_ffx_cas")]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv[hash_name("amd_ffx_cas")].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv[hash_name("amd_ffx_cas")].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv[hash_name("amd_ffx_cas")], nullptr);
		ctx->VSSetShader(g_vs[hash_name("fullscreen_triangle")].get(), nullptr, 0);
		ctx->PSSetShader(g_ps[hash_name("amd_ffx_cas")].get(), nullptr, 0);

		ctx->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

		[[unlikely]] if (!g_cb[hash_name("cb")]) {
			create_constant_buffer(device.get(), sizeof(g_cb_data), g_cb[hash_name("cb")].put());
		}

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("amd_ffx_lfga")]) {
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("amd_ffx_lfga")].put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
		}

		// Create blue noise texture.
		[[unlikely]] if (!g_srv[hash_name("blue_noise_tex")]) {
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
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv[hash_name("blue_noise_tex")].put()), >= 0);
		}

		// Update the constant buffer.
		// We need to limit the temporal grain update rate, otherwise grain will flicker.
		constexpr auto interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / (double)BLUE_NOISE_TEX_ARRAY_SIZE));
		static auto last_update = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		if (now - last_update >= interval) {
			g_cb_data.tex_noise_index += 1.0f;
			if (g_cb_data.tex_noise_index >= (float)BLUE_NOISE_TEX_ARRAY_SIZE) {
				g_cb_data.tex_noise_index = 0.0f;
			}
			update_constant_buffer(ctx, g_cb[hash_name("cb")].get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_ps[hash_name("amd_ffx_lfga")].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(13, 1, &g_cb[hash_name("cb")]);
		const std::array ps_srvs_amd_ffx_lfga = { g_srv[hash_name("amd_ffx_cas")].get(), g_srv[hash_name("blue_noise_tex")].get() };
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
	auto hr = cs->GetPrivateData(g_cs_downsample_depth_0x27BD5265_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_downsample_depth_0x27BD5265_hash) {
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->CSGetShaderResources(0, 1, srv_depth.put());
		srv_depth->GetResource(g_resource[hash_name("depth")].put());
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_taa_0x06BBC941_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0x06BBC941_hash) {
		if (g_enable_dlss) {
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED);
			g_taa_tag = (uintptr_t)ctx;

			ctx->CSGetConstantBuffers(1, 1, g_cb[hash_name("taa_b1")].put());
			ctx->CSGetConstantBuffers(2, 1, g_cb[hash_name("taa_b2")].put());

			// SRV1 should be motion vectors.
			// SRV2 should be the scene.
			// SRV3 should be the buffer ro_postfx_luminance_buffautoexposure.
			std::array<ID3D11ShaderResourceView*, 3> srvs = {};
			ctx->CSGetShaderResources(1, srvs.size(), srvs.data());
			srvs[0]->GetResource(g_resource[hash_name("mvs")].put());
			srvs[1]->GetResource(g_resource[hash_name("scene")].put());
			g_srv[hash_name("ro_postfx_luminance_buffautoexposure")] = srvs[2];

			// UAV1 should be the TAA out.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(1, 1, uav.put());
			uav->GetResource(g_resource[hash_name("taa")].put());

			release_com_array(srvs);
			return true;
		}

		// Create CS.
		[[unlikely]] if (!g_cs[hash_name("TAA_0x06BBC941")]) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());
			const std::string viewport_size_str = std::format("float2({},{})", (float)g_swapchain_width, (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_SIZE", viewport_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(device.get(), g_cs[hash_name("TAA_0x06BBC941")].put(), L"TAA_0x06BBC941_cs.hlsl", "main", defines);
		}

		// Bindings.
		ctx->CSSetShader(g_cs[hash_name("TAA_0x06BBC941")].get(), nullptr, 0);

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
				case g_ps_ssao_0x94445D2D_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_0x94445D2D_guid, sizeof(g_ps_ssao_0x94445D2D_hash), &g_ps_ssao_0x94445D2D_hash), >= 0);
					break;
				case g_ps_tonemap_0xA6F33860_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xA6F33860_guid, sizeof(g_ps_tonemap_0xA6F33860_hash), &g_ps_tonemap_0xA6F33860_hash), >= 0);
					break;
				case g_ps_tonemap_0x11E16EF3_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x11E16EF3_guid, sizeof(g_ps_tonemap_0x11E16EF3_hash), &g_ps_tonemap_0x11E16EF3_hash), >= 0);
					break;
				case g_ps_vignette_0xFD4216EA_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_vignette_0xFD4216EA_guid, sizeof(g_ps_vignette_0xFD4216EA_hash), &g_ps_vignette_0xFD4216EA_hash), >= 0);
					break;
				case g_ps_upsample_0x1A0CD2AE_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_upsample_0x1A0CD2AE_guid, sizeof(g_ps_upsample_0x1A0CD2AE_hash), &g_ps_upsample_0x1A0CD2AE_hash), >= 0);
					break;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xC97890C8_hash), >= 0);
					break;
				case g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_guid, sizeof(g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_hash), &g_ps_motion_blur_and_lens_distortion_mvs_0xE908E905_hash), >= 0);
					break;
			}
		}
		else if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_downsample_depth_0x27BD5265_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_downsample_depth_0x27BD5265_guid, sizeof(g_cs_downsample_depth_0x27BD5265_hash), &g_cs_downsample_depth_0x27BD5265_hash), >= 0);
					break;
				case g_cs_taa_0x06BBC941_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0x06BBC941_guid, sizeof(g_cs_taa_0x06BBC941_hash), &g_cs_taa_0x06BBC941_hash), >= 0);
					break;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_cb[hash_name("taa_b1")]) {
		g_taa_cb1_mapped_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_cb[hash_name("taa_b1")]) {
		std::memcpy(&g_per_view_cb, g_taa_cb1_mapped_data, sizeof(PerViewCB));
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

	auto upgrade_rts = [&]() {
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
		return false;
	};

	// Filter RTs.
	bool result = false;
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		result = upgrade_rts();

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
			result = true;
		}
	}
	if ((desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		result = upgrade_rts();

		// Handle this separatly.
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			result = true;
		}
	}
	return result;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	// Game usese 1, 4 and 16.
	// Force 16 on all samplers.
	desc.max_anisotropy = 16.0f;

	return true;
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
	desc.back_buffer_count = desc.back_buffer_count < 2 ? 2 : desc.back_buffer_count;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.fullscreen_state = false;
	desc.fullscreen_refresh_rate = 0.0f;

	if (g_force_vsync_off) {
		desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		desc.sync_interval = 0;
	}

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	native_swapchain->GetDesc(&desc);

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_enable_dlss) {
		Com_ptr<ID3D11Device> device;
		ensure(native_swapchain->GetDevice(IID_PPV_ARGS(device.put())), >= 0);
		Com_ptr<ID3D11DeviceContext> ctx;
		device->GetImmediateContext(ctx.put());
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
	}

	// Reset resolution dependent resources.
	g_cs[hash_name("xe_gtao_prefilter_depths16x16")].reset();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	g_ps[hash_name("xe_gtao_main_pass")].reset();
	g_cs[hash_name("TAA_0x06BBC941")].reset();
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

	if (g_enable_dlss) {
		DLSS::instance().init(native_device);
	}
}

static void on_destroy_device(reshade::api::device *device)
{
	if (g_enable_dlss) {
		DLSS::instance().shutdown();
	}
	g_rtv.clear();
	g_srv.clear();
	g_vs.clear();
	g_ps.clear();
	g_cs.clear();
	g_smp.clear();
	g_resource.clear();
	g_cb.clear();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DLSSPreset", g_dlss_preset)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DLSSPreset", g_dlss_preset);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "VignetteStrenght", g_vignette_strenght)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "VignetteStrenght", g_vignette_strenght);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "Dishonored2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
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
	#endif

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			DLSS::instance().init(device);
			DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
		}
		else {
			DLSS::instance().shutdown();
		}
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "F", "K" };
	if (ImGui::Combo("DLSS preset", &g_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DLSSPreset", g_dlss_preset);
		DLSS::instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens distortion", &g_disable_lens_distortion)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDistortion", g_disable_lens_distortion);
		g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xC97890C8")].reset();
		g_ps[hash_name("motion_blur_and_lens_distortion_mvs_0xE908E905")].reset();
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens dirt", &g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt);
		g_ps[hash_name("tonemap_0xA6F33860")].reset();
		g_ps[hash_name("tonemap_0x11E16EF3")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("XeGTAO radius", &g_xe_gtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
		g_cs[hash_name("xe_gtao_prefilter_depths16x16")].reset();
		g_ps[hash_name("xe_gtao_main_pass")].reset();
	}
	static constexpr std::array xe_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO quality", &g_xe_gtao_quality, xe_gtao_quality_items.data(), xe_gtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
		g_ps[hash_name("xe_gtao_main_pass")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps[hash_name("tonemap_0xA6F33860")].reset();
		g_ps[hash_name("tonemap_0x11E16EF3")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Vignette strenght", &g_vignette_strenght, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "VignetteStrenght", g_vignette_strenght);
		g_ps[hash_name("vignette")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps[hash_name("amd_ffx_cas")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "TRC", g_trc);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "Gamma", g_gamma);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "Dishonored2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
}

extern "C" __declspec(dllexport) const char* NAME = "Dishonored2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Dishonored2GraphicalUpgrade v1.7.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/Dishonored2GraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			g_graphical_upgrade_path = get_graphical_upgrade_path();
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