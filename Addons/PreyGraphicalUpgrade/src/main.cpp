#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "PreyGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/PreyGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_shadow_mask_0xB01E0A76 = { 0xB01E0A76, { 0xc35b1438, 0xd049, 0x445b, { 0xab, 0x80, 0xfe, 0xfe, 0xbe, 0xfc, 0xb2, 0x61 }}};

constexpr Shader_hash g_ps_ao_0xDB98D83F = { 0xDB98D83F, { 0x900c80a, 0x260a, 0x44fb, { 0x94, 0x44, 0x41, 0x56, 0xf0, 0x60, 0x25, 0xa9 }}};

constexpr Shader_hash g_ps_motion_blur_0x09633436 = { 0x09633436, { 0x9e3ee1f9, 0x9ef7, 0x42b8, { 0x90, 0x1, 0xad, 0x6, 0x4d, 0x5a, 0x77, 0x72 }}};
constexpr Shader_hash g_ps_motion_blur_0x50E44977 = { 0x50E44977, { 0x81ad4432, 0x3700, 0x4750, { 0xab, 0x93, 0x57, 0x44, 0x3e, 0x56, 0x64, 0xf1 }}};
constexpr Shader_hash g_ps_motion_blur_0x9DFDDC83 = { 0x9DFDDC83, { 0x27409c0, 0xb962, 0x4565, { 0x8b, 0x6, 0xcc, 0x47, 0x4, 0x6d, 0x12, 0xbc }}};
constexpr Shader_hash g_ps_motion_blur_0xD0C2257A = { 0xD0C2257A, { 0xec4c025a, 0xc1e4, 0x4713, { 0x94, 0x56, 0x45, 0xf0, 0xca, 0xe1, 0xae, 0x84 }}};

constexpr Shader_hash g_ps_smaa_edge_detection_0x47B723BD = { 0x47B723BD, { 0x8fae771c, 0x8fde, 0x48b8, { 0xa8, 0x3, 0x75, 0x44, 0xa0, 0x33, 0xa, 0x38 }}};
constexpr Shader_hash g_ps_smaa_blending_weight_calculation_0x5636A813 = { 0x5636A813, { 0xbba9d644, 0xfb55, 0x4934, { 0x8b, 0x20, 0xcf, 0x2a, 0x7a, 0x37, 0x2f, 0xfe }}};
constexpr Shader_hash g_ps_smaa_neighborhood_blending_0x2E9A5D4C = { 0x2E9A5D4C, { 0xc38ce36, 0x2d4, 0x42a6, { 0x9c, 0x5a, 0x4, 0x80, 0xbf, 0x83, 0x8, 0x2e }}};
constexpr Shader_hash g_ps_smaa_2tx_0xBF813081 = { 0xBF813081, { 0xa9f436b, 0x8aaa, 0x4c83, { 0xbb, 0xdc, 0x9d, 0xa6, 0xbe, 0x1, 0x4d, 0x93 }}};

constexpr Shader_hash g_ps_post_aa_composite_0x83AE9250 = { 0x83AE9250, { 0xd9b9f937, 0x6ec2, 0x4723, { 0x92, 0x5b, 0x17, 0xed, 0xa, 0x71, 0xd1, 0x2e }}};
constexpr Shader_hash g_ps_post_aa_composite_0xED6287FE = { 0xED6287FE, { 0x9497c921, 0x3a22, 0x4535, { 0x84, 0xdf, 0x49, 0x35, 0x20, 0x5b, 0x51, 0x40 }}};

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
static float g_vigenette_strength = 1.0f;

// GTAO
constexpr size_t GTAO_DEPTH_MIP_LEVELS = 5;
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

#if DEV
static bool g_enable_ao = true;
static bool g_disable_ssdo;
#endif

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
	hr = ps->GetPrivateData(g_ps_post_aa_composite_0x83AE9250.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_post_aa_composite_0x83AE9250.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["post_aa_composite_0x83AE9250"_h]) {
			const std::string vigenette_strength_val = std::to_string(g_vigenette_strength);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGENETTE_STRENGTH", vigenette_strength_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["post_aa_composite_0x83AE9250"_h].put(), L"PostAAComposite_0x83AE9250_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["post_aa_composite_0x83AE9250"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_post_aa_composite_0xED6287FE.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_post_aa_composite_0xED6287FE.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["post_aa_composite_0xED6287FE"_h]) {
			const std::string vigenette_strength_val = std::to_string(g_vigenette_strength);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIGENETTE_STRENGTH", vigenette_strength_val.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["post_aa_composite_0xED6287FE"_h].put(), L"PostAAComposite_0xED6287FE_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["post_aa_composite_0xED6287FE"_h].get(), nullptr, 0);

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

	return false;
}

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
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
				case g_ps_post_aa_composite_0x83AE9250.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_post_aa_composite_0x83AE9250.guid, sizeof(g_ps_post_aa_composite_0x83AE9250.hash), &g_ps_post_aa_composite_0x83AE9250.hash), >= 0);
					return;
				case g_ps_post_aa_composite_0xED6287FE.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_post_aa_composite_0xED6287FE.guid, sizeof(g_ps_post_aa_composite_0xED6287FE.hash), &g_ps_post_aa_composite_0xED6287FE.hash), >= 0);
					return;
				case g_ps_post_aa_composite_0xFAEE5EE9.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_post_aa_composite_0xFAEE5EE9.guid, sizeof(g_ps_post_aa_composite_0xFAEE5EE9.hash), &g_ps_post_aa_composite_0xFAEE5EE9.hash), >= 0);
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

		if (resource_desc.texture.format == reshade::api::format::r32g32_float) {
			desc.format = reshade::api::format::r32g32_float;
			return true;
		}

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

		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}

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
	reset_com_array(g_uav_gtao_prefilter_depths16x16);
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
	if (!reshade::get_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength)) {
		reshade::set_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength);
	}
	if (!reshade::get_config_value(nullptr, NAME, "DisablemotionBlur", g_disable_motion_blur)) {
		reshade::set_config_value(nullptr, NAME, "DisablemotionBlur", g_disable_motion_blur);
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

	if (ImGui::SliderFloat("Vigenette strength", &g_vigenette_strength, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["post_aa_composite_0x496492FE"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0x83AE9250"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0xED6287FE"_h].reset();
		g_managed_resources.pixel_shaders["post_aa_composite_0xFAEE5EE9"_h].reset();
		reshade::set_config_value(nullptr, NAME, "VigenetteStrength", g_vigenette_strength);
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable motion blur", &g_disable_motion_blur)) {
		reshade::set_config_value(nullptr, NAME, "DisablemotionBlur", g_disable_motion_blur);
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
