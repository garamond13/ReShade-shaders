#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "PreyGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v3.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/PreyGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_shadow_mask_0xB01E0A76 = { 0xB01E0A76, { 0xc35b1438, 0xd049, 0x445b, { 0xab, 0x80, 0xfe, 0xfe, 0xbe, 0xfc, 0xb2, 0x61 }}};
constexpr Shader_hash g_ps_ao_0xDB98D83F = { 0xDB98D83F, { 0x900c80a, 0x260a, 0x44fb, { 0x94, 0x44, 0x41, 0x56, 0xf0, 0x60, 0x25, 0xa9 }}};
constexpr Shader_hash g_ps_ssr_0xAED014D7 = { 0xAED014D7, { 0xf0b0baa0, 0x8f0, 0x4d32, { 0x8b, 0xf3, 0x53, 0xb2, 0xf3, 0x1b, 0x1c, 0x25 }}};

// Motion blur.
constexpr Shader_hash g_ps_motion_blur_0x09633436 = { 0x09633436, { 0x9e3ee1f9, 0x9ef7, 0x42b8, { 0x90, 0x1, 0xad, 0x6, 0x4d, 0x5a, 0x77, 0x72 }}};
constexpr Shader_hash g_ps_motion_blur_0x50E44977 = { 0x50E44977, { 0x81ad4432, 0x3700, 0x4750, { 0xab, 0x93, 0x57, 0x44, 0x3e, 0x56, 0x64, 0xf1 }}};
constexpr Shader_hash g_ps_motion_blur_0x9DFDDC83 = { 0x9DFDDC83, { 0x27409c0, 0xb962, 0x4565, { 0x8b, 0x6, 0xcc, 0x47, 0x4, 0x6d, 0x12, 0xbc }}};
constexpr Shader_hash g_ps_motion_blur_0xD0C2257A = { 0xD0C2257A, { 0xec4c025a, 0xc1e4, 0x4713, { 0x94, 0x56, 0x45, 0xf0, 0xca, 0xe1, 0xae, 0x84 }}};

constexpr Shader_hash g_ps_downsample_0xD4E64E5D = { 0xD4E64E5D, { 0x6410d2e6, 0x90d1, 0x4ca9, { 0xad, 0x43, 0xe9, 0xd0, 0x7e, 0x60, 0x6f, 0xa3 }}};
constexpr Shader_hash g_ps_bloom_0x246F473B = { 0x246F473B, { 0xbc52921e, 0x854a, 0x4837, { 0xb6, 0xb9, 0x8a, 0x21, 0x2c, 0xd, 0x7, 0x7a }}};
constexpr Shader_hash g_ps_bloom_0x0A210C5D = { 0x0A210C5D, { 0xea96cc1d, 0x1749, 0x4a95, { 0xa3, 0x78, 0xa2, 0x4c, 0xa2, 0xbb, 0x90, 0x18 }}};
constexpr Shader_hash g_ps_tonemap_0x37ACE8EF = { 0x37ACE8EF, { 0x7c3bbbc4, 0xe87c, 0x4890, { 0x99, 0x84, 0xd2, 0x69, 0x47, 0x22, 0x46, 0x10 }}};
constexpr Shader_hash g_ps_tonemap_0xB5DC761A = { 0xB5DC761A, { 0x452fbbf4, 0x4eda, 0x478e, { 0xb0, 0x61, 0x96, 0xa6, 0x6f, 0xe5, 0xe2, 0xa3 }}};

// SMAA.
constexpr Shader_hash g_ps_smaa_edge_detection_0x47B723BD = { 0x47B723BD, { 0x8fae771c, 0x8fde, 0x48b8, { 0xa8, 0x3, 0x75, 0x44, 0xa0, 0x33, 0xa, 0x38 }}};
constexpr Shader_hash g_ps_smaa_blending_weight_calculation_0x5636A813 = { 0x5636A813, { 0xbba9d644, 0xfb55, 0x4934, { 0x8b, 0x20, 0xcf, 0x2a, 0x7a, 0x37, 0x2f, 0xfe }}};
constexpr Shader_hash g_ps_smaa_neighborhood_blending_0x2E9A5D4C = { 0x2E9A5D4C, { 0xc38ce36, 0x2d4, 0x42a6, { 0x9c, 0x5a, 0x4, 0x80, 0xbf, 0x83, 0x8, 0x2e }}};
constexpr Shader_hash g_ps_smaa_2tx_0xBF813081 = { 0xBF813081, { 0xa9f436b, 0x8aaa, 0x4c83, { 0xbb, 0xdc, 0x9d, 0xa6, 0xbe, 0x1, 0x4d, 0x93 }}};

constexpr Shader_hash g_ps_lens_effects_0x51F2811A = { 0x51F2811A, { 0x85c3a7b9, 0x4dc1, 0x476f, { 0xa3, 0x20, 0xba, 0xcb, 0x24, 0xb2, 0x2c, 0x92 }}};
constexpr Shader_hash g_ps_lens_effects_0xDAA20F29 = { 0xDAA20F29, { 0xb2c0b0c4, 0xefeb, 0x4595, { 0xbf, 0xf1, 0xa5, 0xd3, 0x5d, 0x9f, 0xd, 0xdb }}};

// Only when "SMAA 2TX" is on.
constexpr Shader_hash g_ps_post_aa_composite_0x496492FE = { 0x496492FE, { 0x9fe2e596, 0x5011, 0x4266, { 0xa6, 0x9c, 0x29, 0xef, 0x29, 0xa4, 0x22, 0x23 }}};
constexpr Shader_hash g_ps_post_aa_composite_0xFAEE5EE9 = { 0xFAEE5EE9, { 0x747cfe68, 0x6d13, 0x45a0, { 0x8a, 0xb, 0x3d, 0x53, 0xa6, 0x21, 0xb3, 0xce }}};

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
static bool g_disable_motion_blur = true;
static bool g_disable_lens_effects = true;
static float g_vigenette_strength = 1.0f;

// GTAO
constexpr size_t GTAO_DEPTH_MIP_LEVELS = 5;
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

#if DEV
static bool g_enable_ao = true;
static bool g_disable_ssdo;
#endif

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;
static int g_bloom_input_width;
static int g_bloom_input_height;

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

// Device resources.
static std::array<ID3D11UnorderedAccessView*, GTAO_DEPTH_MIP_LEVELS> g_uav_gtao_prefilter_depths16x16;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_x;

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
	hr = ps->GetPrivateData(g_ps_ssr_0xAED014D7.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssr_0xAED014D7.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["ssr_0xAED014D7"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["ssr_0xAED014D7"_h].put(), L"SSR_0xAED014D7_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["ssr_0xAED014D7"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_shadow_mask_0xB01E0A76.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_shadow_mask_0xB01E0A76.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h].put(), L"ShadowMask_0xB01E0A76_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_ao_0xDB98D83F.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ao_0xDB98D83F.hash) {
		#if DEV
		if (!g_enable_ao) {
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);

			#if DEV && SHOW_AO
			Com_ptr<ID3D11Resource> resource;
			rtv->GetResource(g_managed_resources.resources["scene"_h].put());
			#endif

			if (g_disable_ssdo) {
				constexpr FLOAT clear_color[4] = { 0.5f, 0.5f, 0.5f, 0.0f }; // xyz - encoded bent normals, w - obscurance.
				ctx->ClearRenderTargetView(rtv.get(), clear_color);
				return true;
			}
			return false;
		}
		#endif

		// GTAOPrefilterDepths16x16 pass
		//

		Com_ptr<ID3D11Buffer> cb_13;
		ctx->PSGetConstantBuffers(13, 1, cb_13.put());
		Com_ptr<ID3D11Buffer> cb_0;
		ctx->PSGetConstantBuffers(0, 1, cb_0.put());

		// Get linearized depth.
		Com_ptr<ID3D11ShaderResourceView> srv_depth;
		ctx->PSGetShaderResources(1, 1, srv_depth.put());

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths16x16_cs", defines);
		}

		// Create sampler.
		[[unlikely]] if (!g_managed_resources.samplers["point_clamp"_h]) {
			create_sampler_point_clamp(g_device, g_managed_resources.samplers["point_clamp"_h].put());
		}

		// Create prefilter depths views.
		[[unlikely]] if (!g_uav_gtao_prefilter_depths16x16[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = GTAO_DEPTH_MIP_LEVELS;
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
			for (int i = 0; i < g_uav_gtao_prefilter_depths16x16.size(); ++i) {
			   uav_desc.Texture2D.MipSlice = i;
			   ensure(g_device->CreateUnorderedAccessView(tex.get(), &uav_desc, &g_uav_gtao_prefilter_depths16x16[i]), >= 0);
			}

			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_prefilter_depths16x16"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, g_uav_gtao_prefilter_depths16x16.size(), g_uav_gtao_prefilter_depths16x16.data(), nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(13, 1, &cb_13);
		ctx->CSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
		ctx->CSSetShaderResources(0, 1, &srv_depth);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, g_uav_gtao_prefilter_depths16x16.size()> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//
		
		// GTAOMainPass pass
		//
		// We will render to the original RT (rgba8_unorm).
		// Also we will skip denoise here and rely on the game's denoiser instead.
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_main_pass"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string gtao_quality_val = std::to_string(g_gtao_quality);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "GTAO_QUALITY", gtao_quality_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_main_pass"_h].put(), L"GTAO_impl.hlsl", "main_pass_ps", defines);
		}

		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		++g_cb_data.gtao_temporal_index;
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["gtao_main_pass"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->PSSetShaderResources(1, 1, &g_managed_resources.shader_resource_views["gtao_prefilter_depths16x16"_h]);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		#if DEV && SHOW_AO
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource;
		rtv->GetResource(g_managed_resources.resources["scene"_h].put());
		#endif

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_0x09633436.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_0x09633436.hash) {
		if (g_disable_motion_blur) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_0x50E44977.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_0x50E44977.hash) {
		if (g_disable_motion_blur) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_0x9DFDDC83.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_0x9DFDDC83.hash) {
		if (g_disable_motion_blur) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_motion_blur_0xD0C2257A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_motion_blur_0xD0C2257A.hash) {
		if (g_disable_motion_blur) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_downsample_0xD4E64E5D.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_downsample_0xD4E64E5D.hash) {

		// We just need SRV0 (half resolution scene with killed fireflies),
		// do the original draw first so we don't need to do full restore later.
		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("0xD4E64E5D: The expected primitive topology wasn't what we expected it to be!");
		}
		#endif

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup rasterizer.
		Com_ptr<ID3D11RasterizerState> rasterizer;
		ctx->RSGetState(rasterizer.put());

		// Backup Blend.
		Com_ptr<ID3D11BlendState> blend_original;
		FLOAT blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Get bloom input width and height.
		[[unlikely]] if (!g_bloom_input_width) {
			// Get SRV0's texture description.
			Com_ptr<ID3D11ShaderResourceView> srv;
			ctx->PSGetShaderResources(0, 1, srv.put());
			Com_ptr<ID3D11Resource> resource;
			srv->GetResource(resource.put());
			Com_ptr<ID3D11Texture2D> tex;
			ensure(resource->QueryInterface(tex.put()), >= 0);
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			g_bloom_input_width = tex_desc.Width;
			g_bloom_input_height = tex_desc.Height;
		}

		// Create MIPs and views.
		//

		const UINT y_mip0_width = g_bloom_input_width >> 1;
		const UINT y_mip0_height = g_bloom_input_height >> 1;

		// Create Y MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_y[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = y_mip0_width;
			tex_desc.Height = y_mip0_height;
			tex_desc.MipLevels = g_bloom_nmips;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = tex_desc.Format;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			for (int i = 0; i < g_bloom_nmips; ++i) {
				rtv_desc.Texture2D.MipSlice = i;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_bloom_mips_y[i]), >= 0);
				srv_desc.Texture2D.MostDetailedMip = i;
				ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_bloom_mips_y[i]), >= 0);
			}
		}

		const UINT x_mip0_width = g_bloom_input_width >> 1;
		const UINT x_mip0_height = g_bloom_input_height;

		// Create X MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_x[0]) {
			// Create X MIP0 and views.
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = x_mip0_width;
			tex_desc.Height = x_mip0_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[0]), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[0]), >= 0);

			// Create rest of X MIPs and views.
			for (UINT i = 1; i < g_bloom_nmips; ++i) {
				tex_desc.Width = std::max(1u, x_mip0_width >> i);
				tex_desc.Height = std::max(1u, x_mip0_height >> i);
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[i]), >= 0);
				ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[i]), >= 0);
			}
		}

		//

		// Prefilter + downsample pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(g_device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_downsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create CB.
		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		D3D11_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(g_swapchain_width, g_swapchain_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[0], nullptr);
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->RSSetState(nullptr);
		ctx->RSSetViewports(1, &viewport_x);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);

		// Draw X pass.
		ctx->Draw(3, 0);

		std::vector<D3D11_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(x_mip0_width, x_mip0_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(0.0f, 1.0f);
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[0], nullptr);
		ctx->RSSetViewports(1, &viewports_y[0]);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);

		// Draw Y pass.
		ctx->Draw(3, 0);

		//

		// Downsample passes
		//

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_data.axis = float2(1.0f, 0.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			g_cb_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[i], nullptr);
			ctx->RSSetViewports(1, &viewport_x);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i - 1]);

			// Draw X pass.
			ctx->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i], nullptr);
			ctx->RSSetViewports(1, &viewports_y[i]);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);

			// Draw Y pass.
			ctx->Draw(3, 0);
		}

		//

		// Upsample passes
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_upsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_upsample"_h].put(), L"Bloom_impl.hlsl", "upsample_ps");
		}

		// Create blend.
		[[unlikely]] if (!g_managed_resources.blends["bloom"_h]) {
			CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
			ensure(g_device->CreateBlendState(&desc, g_managed_resources.blends["bloom"_h].put()), >= 0);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_upsample"_h].get(), nullptr, 0);

		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// If both dst and src are D3D11_BLEND_BLEND_FACTOR,
			// factor of 0.5 will be enegrgy preserving.
			static constexpr FLOAT blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i - 1], nullptr);
			ctx->RSSetViewports(1, &viewports_y[i - 1]);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i]);
			ctx->OMSetBlendState(g_managed_resources.blends["bloom"_h].get(), blend_factor, UINT_MAX);

			ctx->Draw(3, 0);
		}

		//

		// Restore.
		// Rasterizer must be restored, others maybe.
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->RSSetState(rasterizer.get());
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x246F473B.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x246F473B.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x0A210C5D.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x0A210C5D.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x37ACE8EF.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x37ACE8EF.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x37ACE8EF"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0x37ACE8EF"_h].put(), L"Tonemap_0x37ACE8EF_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x37ACE8EF"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(2, 1, &g_srv_bloom_mips_y[0]);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xB5DC761A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xB5DC761A.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0xB5DC761A"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0xB5DC761A"_h].put(), L"Tonemap_0xB5DC761A_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0xB5DC761A"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(2, 1, &g_srv_bloom_mips_y[0]);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_smaa_edge_detection_0x47B723BD.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_smaa_edge_detection_0x47B723BD.hash) {
		if (g_enable_dlss) {
			#if !SHOW_AO
			Com_ptr<ID3D11ShaderResourceView> srv;
			ctx->PSGetShaderResources(0, 1, srv.put());
			srv->GetResource(g_managed_resources.resources["scene"_h].put());
			#endif
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_smaa_blending_weight_calculation_0x5636A813.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_smaa_blending_weight_calculation_0x5636A813.hash) {
		if (g_enable_dlss) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_smaa_neighborhood_blending_0x2E9A5D4C.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_smaa_neighborhood_blending_0x2E9A5D4C.hash) {
		if (g_enable_dlss) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_smaa_2tx_0xBF813081.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_smaa_2tx_0xBF813081.hash) {
		if (g_enable_dlss) {
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// Get RTV resource.
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			Com_ptr<ID3D11Resource> resource_rt;
			rtv->GetResource(resource_rt.put());

			// MVs pass
			//

			// Create PS.
			[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_2tx_0xBF813081"_h]) {
				create_pixel_shader(g_device, g_managed_resources.pixel_shaders["smaa_2tx_0xBF813081"_h].put(), L"SMAA2TX_0xBF813081_ps.hlsl");
			}

			// Create RTV.
			[[unlikely]] if (!g_managed_resources.render_target_views["mvs"_h]) {
				// Get dynamic objects MVs texture description.
				Com_ptr<ID3D11ShaderResourceView> srv;
				ctx->PSGetShaderResources(3, 1, srv.put());
				Com_ptr<ID3D11Resource> resource;
				srv->GetResource(resource.put());
				ensure(resource->QueryInterface(g_managed_resources.textures_2d["mvs"_h].put()), >= 0);
				D3D11_TEXTURE2D_DESC tex_desc;
				g_managed_resources.textures_2d["mvs"_h]->GetDesc(&tex_desc);

				// Create MVs texture and RTV.
				tex_desc.Format = DXGI_FORMAT_R32G32_FLOAT;			
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["mvs"_h].put()), >= 0);
				ensure(g_device->CreateRenderTargetView(g_managed_resources.textures_2d["mvs"_h].get(), nullptr, g_managed_resources.render_target_views["mvs"_h].put()), >= 0);
			}

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["mvs"_h], nullptr);
			ctx->PSSetShader(g_managed_resources.pixel_shaders["smaa_2tx_0xBF813081"_h].get(), nullptr, 0);

			cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

			//

			// DLSS pass
			//

			// Create texture.
			// The original RT resource doesn't have D3D11_BIND_UNORDERED_ACCESS bind flag needed for DLSS.
			[[unlikely]] if (!g_managed_resources.textures_2d["dlss_output"_h]) {
				// Get original RT texture description.
				ensure(resource_rt->QueryInterface(g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
				D3D11_TEXTURE2D_DESC tex_desc;
				g_managed_resources.textures_2d["dlss_output"_h]->GetDesc(&tex_desc);

				// Create DLSS output.
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
			}

			// Get depth resource.
			Com_ptr<ID3D11ShaderResourceView> srv_depth;
			ctx->PSGetShaderResources(16, 1, srv_depth.put());
			Com_ptr<ID3D11Resource> resource_depth;
			srv_depth->GetResource(resource_depth.put());

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = g_managed_resources.resources["scene"_h].get();
			eval_params.Feature.pInOutput = g_managed_resources.textures_2d["dlss_output"_h].get();
			eval_params.pInDepth = resource_depth.get();
			eval_params.pInMotionVectors = g_managed_resources.textures_2d["mvs"_h].get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			eval_params.InMVScaleX = g_swapchain_width;
			eval_params.InMVScaleY = g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
			eval_params.InJitterOffsetX = g_jitter_x * g_swapchain_width * -0.5;
			eval_params.InJitterOffsetY = g_jitter_y * g_swapchain_height * 0.5;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			// Copy DLSS output to the original output.
			ctx->CopyResource(resource_rt.get(), g_managed_resources.textures_2d["dlss_output"_h].get());

			//

			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_post_aa_composite_0x496492FE.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_post_aa_composite_0x496492FE.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["post_aa_composite_0x496492FE"_h]) {
			const std::string vigenette_strength_val = std::to_string(g_vigenette_strength);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGENETTE_STRENGTH", vigenette_strength_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["post_aa_composite_0x496492FE"_h].put(), L"PostAAComposite_0x496492FE_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["post_aa_composite_0x496492FE"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_post_aa_composite_0xFAEE5EE9.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_post_aa_composite_0xFAEE5EE9.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["post_aa_composite_0xFAEE5EE9"_h]) {
			const std::string vigenette_strength_val = std::to_string(g_vigenette_strength);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGENETTE_STRENGTH", vigenette_strength_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["post_aa_composite_0xFAEE5EE9"_h].put(), L"PostAAComposite_0xFAEE5EE9_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["post_aa_composite_0xFAEE5EE9"_h].get(), nullptr, 0);

		return false;
	}

	// Draws multiple effects, lens flare should be the last (6th).
	// TODO: Disable lens flare separatly?
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_lens_effects_0x51F2811A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_lens_effects_0x51F2811A.hash) {
		if (g_disable_lens_effects) {
			return true;
		}
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

	#if DEV
	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_shadow_mask_0xB01E0A76.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_shadow_mask_0xB01E0A76.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h].put(), L"ShadowMask_0xB01E0A76_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["shadow_mask_0xB01E0A76"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_lens_effects_0xDAA20F29.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_lens_effects_0xDAA20F29.hash) {
		if (g_disable_lens_effects) {
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
				case g_ps_ssr_0xAED014D7.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssr_0xAED014D7.guid, sizeof(g_ps_ssr_0xAED014D7.hash), &g_ps_ssr_0xAED014D7.hash), >= 0);
					return;
				case g_ps_shadow_mask_0xB01E0A76.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_shadow_mask_0xB01E0A76.guid, sizeof(g_ps_shadow_mask_0xB01E0A76.hash), &g_ps_shadow_mask_0xB01E0A76.hash), >= 0);
					return;
				case g_ps_ao_0xDB98D83F.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ao_0xDB98D83F.guid, sizeof(g_ps_ao_0xDB98D83F.hash), &g_ps_ao_0xDB98D83F.hash), >= 0);
					return;
				case g_ps_motion_blur_0x09633436.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_0x09633436.guid, sizeof(g_ps_motion_blur_0x09633436.hash), &g_ps_motion_blur_0x09633436.hash), >= 0);
					return;
				case g_ps_motion_blur_0x50E44977.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_0x50E44977.guid, sizeof(g_ps_motion_blur_0x50E44977.hash), &g_ps_motion_blur_0x50E44977.hash), >= 0);
					return;
				case g_ps_motion_blur_0x9DFDDC83.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_0x9DFDDC83.guid, sizeof(g_ps_motion_blur_0x9DFDDC83.hash), &g_ps_motion_blur_0x9DFDDC83.hash), >= 0);
					return;
				case g_ps_motion_blur_0xD0C2257A.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_motion_blur_0xD0C2257A.guid, sizeof(g_ps_motion_blur_0xD0C2257A.hash), &g_ps_motion_blur_0xD0C2257A.hash), >= 0);
					return;
				case g_ps_smaa_edge_detection_0x47B723BD.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_smaa_edge_detection_0x47B723BD.guid, sizeof(g_ps_smaa_edge_detection_0x47B723BD.hash), &g_ps_smaa_edge_detection_0x47B723BD.hash), >= 0);
					return;
				case g_ps_smaa_blending_weight_calculation_0x5636A813.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_smaa_blending_weight_calculation_0x5636A813.guid, sizeof(g_ps_smaa_blending_weight_calculation_0x5636A813.hash), &g_ps_smaa_blending_weight_calculation_0x5636A813.hash), >= 0);
					return;
				case g_ps_smaa_neighborhood_blending_0x2E9A5D4C.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_smaa_neighborhood_blending_0x2E9A5D4C.guid, sizeof(g_ps_smaa_neighborhood_blending_0x2E9A5D4C.hash), &g_ps_smaa_neighborhood_blending_0x2E9A5D4C.hash), >= 0);
					return;
				case g_ps_smaa_2tx_0xBF813081.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_smaa_2tx_0xBF813081.guid, sizeof(g_ps_smaa_2tx_0xBF813081.hash), &g_ps_smaa_2tx_0xBF813081.hash), >= 0);
					return;
				case g_ps_post_aa_composite_0x496492FE.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_post_aa_composite_0x496492FE.guid, sizeof(g_ps_post_aa_composite_0x496492FE.hash), &g_ps_post_aa_composite_0x496492FE.hash), >= 0);
					return;
				case g_ps_post_aa_composite_0xFAEE5EE9.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_post_aa_composite_0xFAEE5EE9.guid, sizeof(g_ps_post_aa_composite_0xFAEE5EE9.hash), &g_ps_post_aa_composite_0xFAEE5EE9.hash), >= 0);
					return;
				case g_ps_lens_effects_0x51F2811A.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_lens_effects_0x51F2811A.guid, sizeof(g_ps_lens_effects_0x51F2811A.hash), &g_ps_lens_effects_0x51F2811A.hash), >= 0);
					return;
				case g_ps_lens_effects_0xDAA20F29.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_lens_effects_0xDAA20F29.guid, sizeof(g_ps_lens_effects_0xDAA20F29.hash), &g_ps_lens_effects_0xDAA20F29.hash), >= 0);
					return;
				case g_ps_tonemap_0x37ACE8EF.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x37ACE8EF.guid, sizeof(g_ps_tonemap_0x37ACE8EF.hash), &g_ps_tonemap_0x37ACE8EF.hash), >= 0);
					return;
				case g_ps_tonemap_0xB5DC761A.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xB5DC761A.guid, sizeof(g_ps_tonemap_0xB5DC761A.hash), &g_ps_tonemap_0xB5DC761A.hash), >= 0);
					return;
				case g_ps_downsample_0xD4E64E5D.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_downsample_0xD4E64E5D.guid, sizeof(g_ps_downsample_0xD4E64E5D.hash), &g_ps_downsample_0xD4E64E5D.hash), >= 0);
					return;
				case g_ps_bloom_0x246F473B.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x246F473B.guid, sizeof(g_ps_bloom_0x246F473B.hash), &g_ps_bloom_0x246F473B.hash), >= 0);
					return;
				case g_ps_bloom_0x0A210C5D.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x0A210C5D.guid, sizeof(g_ps_bloom_0x0A210C5D.hash), &g_ps_bloom_0x0A210C5D.hash), >= 0);
					return;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	D3D11_BUFFER_DESC desc;
	buffer->GetDesc(&desc);
	if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 1024) {
		g_mapped_cb_handle = resource.handle;
		g_mapped_cb_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	if (g_mapped_cb_handle == resource.handle) {
		auto data = (float*)g_mapped_cb_data;
		// At offset 68 should be transposed projection matrix.
		// This should be reliable.
		if (data[68] && !data[69] && data[70] && !data[71] && !data[72] && data[73] && data[74] && !data[75]) {
			g_jitter_x = data[70]; // m02
			g_jitter_y = data[74]; // m12
		}
		g_mapped_cb_handle = 0;
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

		//if (resource_desc.texture.format == reshade::api::format::r32g32_float) {
		//	desc.format = reshade::api::format::r32g32_float;
		//	return true;
		//}

		//if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_unorm) {
		//	desc.format = reshade::api::format::r16g16b16a16_unorm;
		//	return true;
		//}
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

		// Breaking SSR.
		//if (desc.texture.format == reshade::api::format::r16g16_float) {
		//	desc.texture.format = reshade::api::format::r32g32_float;
		//	return true;
		//}

		// AO output gets stuck in pause menu.
		//if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
		//	desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
		//	return true;
		//}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// The game is not always using x16 even if set.
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
	g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].reset();
	reset_com_array(g_uav_gtao_prefilter_depths16x16);
	g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
	reset_com_array(g_rtv_bloom_mips_y);
	reset_com_array(g_srv_bloom_mips_y);
	reset_com_array(g_rtv_bloom_mips_x);
	reset_com_array(g_srv_bloom_mips_x);
	g_bloom_input_width = 0;
	g_bloom_input_height = 0;
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
	release_com_array(g_uav_gtao_prefilter_depths16x16);
	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
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

	if (!reshade::get_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality)) {
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength)) {
		reshade::set_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength);
	}
	if (!reshade::get_config_value(nullptr, NAME, "DisableLensEffects", g_disable_motion_blur)) {
		reshade::set_config_value(nullptr, NAME, "DisableLensEffects", g_disable_motion_blur);
	}
	if (!reshade::get_config_value(nullptr, NAME, "DisableMotionBlur", g_disable_motion_blur)) {
		reshade::set_config_value(nullptr, NAME, "DisableMotionBlur", g_disable_motion_blur);
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

	if (ImGui::Checkbox("Enable AO", &g_enable_ao))
	{
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
	}
	if (ImGui::Checkbox("Disable SSDO", &g_disable_ssdo))
	{
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
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
	ImGui::Spacing();

	// For whatever reason changing Bloom nmips is crashing the game. Race condition?
	//if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1, 10)) {
	//	release_com_array(g_rtv_bloom_mips_y);
	//	release_com_array(g_srv_bloom_mips_y);
	//	release_com_array(g_rtv_bloom_mips_x);
	//	release_com_array(g_srv_bloom_mips_x);
	//	g_rtv_bloom_mips_y.resize(g_bloom_nmips);
	//	g_srv_bloom_mips_y.resize(g_bloom_nmips);
	//	g_rtv_bloom_mips_x.resize(g_bloom_nmips);
	//	g_srv_bloom_mips_x.resize(g_bloom_nmips);
	//	g_bloom_sigmas.resize(g_bloom_nmips);
	//}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
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

	static constexpr std::array gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("GTAO quality", &g_gtao_quality, gtao_quality_items.data(), gtao_quality_items.size())) {
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["tonemap_0x37ACE8EF"_h].reset();
		g_managed_resources.pixel_shaders["tonemap_0xB5DC761A"_h].reset();
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Vigenette strength", &g_vigenette_strength, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["post_aa_composite_0x496492FE"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0x83AE9250"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0xED6287FE"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0xFAEE5EE9"_h].reset();
		reshade::set_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength);
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens effects", &g_disable_lens_effects)) {
		reshade::set_config_value(nullptr, NAME, "DisableLensEffects", g_disable_lens_effects);
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable motion blur", &g_disable_motion_blur)) {
		reshade::set_config_value(nullptr, NAME, "DisableMotionBlur", g_disable_motion_blur);
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

			// Bloom.
			g_bloom_nmips = 6;
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 1.0f; // 1.0 for the first MIP here is fine cause input is prefiltered.
			g_bloom_sigmas[1] = 2.0f;
			g_bloom_sigmas[2] = 2.0f;
			g_bloom_sigmas[3] = 2.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
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
