#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "AreaTex.h"
#include "SearchTex.h"
#include "Helpers.h"
#include "TRC.h"
#include "HLSLTypes.h"
#include "BlueNoiseTex.h"

struct alignas(16) CB_data
{
   float2 src_size;
   float2 inv_src_size;
   float2 axis;
   float sigma;
   float tex_noise_index;
};

// Shader hooks.
//

constexpr uint32_t g_ps_copy_0xB8813A2F_hash = 0xB8813A2F;
constexpr GUID g_ps_copy_0xB8813A2F_guid = { 0x822c1198, 0xc582, 0x4ac9, { 0xaf, 0xa6, 0xaa, 0x51, 0x7f, 0xbd, 0x6c, 0x98 } };

constexpr uint32_t g_ps_ssao_0x7A054979_hash = 0x7A054979;
constexpr GUID g_ps_ssao_0x7A054979_guid = { 0x27c6d923, 0x7cd8, 0x4176, { 0x98, 0xf8, 0xf9, 0xd6, 0xbf, 0xd3, 0x10, 0xb5 } };

constexpr uint32_t g_cs_ssao_denoise_0x54A9A847_hash = 0x54A9A847;
constexpr GUID g_cs_ssao_denoise_0x54A9A847_guid = { 0xab481a5b, 0x9389, 0x426d, { 0xa1, 0x79, 0xaf, 0xfd, 0xc8, 0x97, 0xce, 0x12 } };

constexpr uint32_t g_cs_ssao_denoise_0x8A03353C_hash = 0x8A03353C;
constexpr GUID g_cs_ssao_denoise_0x8A03353C_guid = { 0x73758e30, 0xf9f3, 0x47a3, { 0x91, 0xc8, 0x9f, 0xd6, 0x47, 0x49, 0x49, 0xe } };

constexpr uint32_t g_ps_bloom_prefilter_0xB357A376_hash = 0xB357A376;
constexpr GUID g_ps_bloom_prefilter_0xB357A376_guid = { 0x2a7b3232, 0x2f8b, 0x4fb4, { 0x9f, 0xf5, 0x9e, 0x2a, 0x13, 0xef, 0x68, 0xfe } };

constexpr uint32_t g_ps_bloom_add_0x24314FFA_hash = 0x24314FFA;
constexpr GUID g_ps_bloom_add_0x24314FFA_guid = { 0xf4b5f60e, 0x3fc2, 0x4ce0, { 0xa9, 0x6b, 0x81, 0x17, 0x33, 0x9f, 0xd8, 0x77 } };

constexpr uint32_t g_ps_bloom_0xEA5892CE_hash = 0xEA5892CE;
constexpr GUID g_ps_bloom_0xEA5892CE_guid = { 0x5ccaaf97, 0xfa5b, 0x4ec4, { 0xb8, 0x6f, 0x74, 0x51, 0xce, 0x3b, 0xd3, 0x1e } };

constexpr uint32_t g_ps_bloom_0x807FB946_hash = 0x807FB946;
constexpr GUID g_ps_bloom_0x807FB946_guid = { 0xb1a5a0e8, 0x33a0, 0x433b, { 0xa0, 0x1f, 0x7a, 0x46, 0xc0, 0x9, 0xb8, 0xbd } };

constexpr uint32_t g_ps_bloom_0x81EC9518_hash = 0x81EC9518;
constexpr GUID g_ps_bloom_0x81EC9518_guid = { 0x7e0de7f9, 0x97b5, 0x4049, { 0xbf, 0x55, 0xee, 0x56, 0xce, 0x87, 0x10, 0x6a } };

constexpr uint32_t g_ps_color_grading_and_vignette_0xBFF40A4D_hash = 0xBFF40A4D;
constexpr GUID g_ps_color_grading_and_vignette_0xBFF40A4D_guid = { 0x8b0816c6, 0xca0, 0x40be, { 0xb9, 0x4a, 0xb, 0x57, 0x3a, 0xe8, 0xd0, 0x58 } };

//

static uint32_t g_swapchain_width;
static uint32_t g_swapchain_height;
static bool g_force_vsync_off = true;
static CB_data g_cb_data;
static SMAA_rt_metrics g_smaa_rt_metrics;
static float g_amd_ffx_cas_sharpness = 0.4f;
static float g_amd_ffx_lfga_amount = 0.35f;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static float g_xe_gtao_radius = 0.35f;
static int g_xe_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
static bool g_has_drawn_ssao;
static bool g_has_drawn_xe_gtao;

#if DEV
Com_ptr<ID3D11ShaderResourceView> g_srv_ao;
#endif

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

// COM resources.
static std::unordered_map<uint32_t, Com_ptr<ID3D11RenderTargetView>> g_rtv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11UnorderedAccessView>> g_uav;
static std::unordered_map<uint32_t, Com_ptr<ID3D11ShaderResourceView>> g_srv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11VertexShader>> g_vs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11PixelShader>> g_ps;
static std::unordered_map<uint32_t, Com_ptr<ID3D11ComputeShader>> g_cs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11SamplerState>> g_smp;
static std::unordered_map<uint32_t, Com_ptr<ID3D11Buffer>> g_cb;
static std::unordered_map<uint32_t, Com_ptr<ID3D11BlendState>> g_blend;
static std::unordered_map<uint32_t, Com_ptr<ID3D11DepthStencilView>> g_dsv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11DepthStencilState>> g_ds;
static std::array<ID3D11UnorderedAccessView*, XE_GTAO_DEPTH_MIP_LEVELS> g_uav_xe_gtao_prefilter_depths16x16;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_x;

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	g_has_drawn_ssao = false;
	g_has_drawn_xe_gtao = false;

	// FPS limiter.
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
	auto hr = ps->GetPrivateData(g_ps_ssao_0x7A054979_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_0x7A054979_hash) {
		ctx->PSGetConstantBuffers(2, 1, g_cb["ssao"_h].put());
		ctx->PSGetShaderResources(0, 1, g_srv["depth"_h].put());

		// Signal the XeGTAO that the game wants to draw AO.
		g_has_drawn_ssao = true;

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_copy_0xB8813A2F_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_copy_0xB8813A2F_hash) {
		if (g_has_drawn_ssao && !g_has_drawn_xe_gtao) {
			Com_ptr<ID3D11Device> device;
			ctx->GetDevice(device.put());

			// RT should be viewspace normal.
			// Originaly the game wants to store AO term in alpha channel of the RT, so we do the same.
			Com_ptr<ID3D11RenderTargetView> rtv_normal;
			ctx->OMGetRenderTargets(1, rtv_normal.put(), nullptr);
			Com_ptr<ID3D11Resource> resource;
			rtv_normal->GetResource(resource.put());

			// Create SRV and UAV normal.
			Com_ptr<ID3D11ShaderResourceView> srv_normal;
			ensure(device->CreateShaderResourceView(resource.get(), nullptr, srv_normal.put()), >= 0);
			Com_ptr<ID3D11UnorderedAccessView> uav_normal;
			ensure(device->CreateUnorderedAccessView(resource.get(), nullptr, uav_normal.put()), >= 0);

			// XeGTAOPrefilterDepths16x16 pass
			//

			// Create CS.
			[[unlikely]] if (!g_cs["xe_gtao_prefilter_depths16x16"_h]) {
				const std::string effect_radius_str = std::to_string(g_xe_gtao_radius);
				const D3D_SHADER_MACRO defines[] = {
					{ "EFFECT_RADIUS", effect_radius_str.c_str() },
					{ nullptr, nullptr }
				};
				create_compute_shader(device.get(), g_cs["xe_gtao_prefilter_depths16x16"_h].put(), L"XeGTAO_impl.hlsl", "prefilter_depths16x16_cs", defines);
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
			ctx->CSSetConstantBuffers(0, 1, &g_cb["ssao"_h]);
			ctx->CSSetSamplers(0, 1, &g_smp["point_clamp"_h]);
			ctx->CSSetShaderResources(0, 1, &g_srv["depth"_h]);

			ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

			// Unbind UAVs.
			static constexpr std::array<ID3D11UnorderedAccessView*, g_uav_xe_gtao_prefilter_depths16x16.size()> uav_nulls_prefilter_depths_pass = {};
			ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

			//

			// XeGTAOMainPass pass
			//

			// Create CS.
			[[unlikely]] if (!g_cs["xe_gtao_main_pass"_h]) {
				const std::string xe_gtao_quality_val = std::to_string(g_xe_gtao_quality);
				const std::string effect_radius_val = std::to_string(g_xe_gtao_radius);
				const D3D_SHADER_MACRO defines[] = {
					{ "EFFECT_RADIUS", effect_radius_val.c_str() },
					{ "XE_GTAO_QUALITY", xe_gtao_quality_val.c_str() },
					{ nullptr, nullptr }
				};
				create_compute_shader(device.get(), g_cs["xe_gtao_main_pass"_h].put(), L"XeGTAO_impl.hlsl", "main_pass_cs", defines);
			}

			// Create AO term and Edges views.
			[[unlikely]] if (!g_uav["xe_gtao_main_pass"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				Com_ptr<ID3D11Texture2D> tex;
				ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, g_uav["xe_gtao_main_pass"_h].put()), >= 0);
				ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["xe_gtao_main_pass"_h].put()), >= 0);
			}

			// Bindings.
			ctx->OMSetRenderTargets(0, nullptr, nullptr); // We have to unbind RTV normal.
			ctx->CSSetUnorderedAccessViews(0, 1, &g_uav["xe_gtao_main_pass"_h], nullptr);
			ctx->CSSetShader(g_cs["xe_gtao_main_pass"_h].get(), nullptr, 0);
			const std::array srvs_main_pass = { g_srv["xe_gtao_prefilter_depths16x16"_h].get(), srv_normal.get() };
			ctx->CSSetShaderResources(0, srvs_main_pass.size(), srvs_main_pass.data());

			ctx->Dispatch((g_swapchain_width + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, (g_swapchain_height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);

			//

			// Doing 2 XeGTAODenoisePass passes correspond to "Denoising level: Medium" from the XeGTAO demo.

			// XeGTAODenoisePass1 pass
			//

			// Create CS.
			[[unlikely]] if (!g_cs["xe_gtao_denoise_pass1"_h]) {
				static constexpr D3D_SHADER_MACRO defines[] = {
					{ "XE_GTAO_FINAL_APPLY", "0" },
					{ nullptr, nullptr }
				};
				create_compute_shader(device.get(), g_cs["xe_gtao_denoise_pass1"_h].put(), L"XeGTAO_impl.hlsl", "denoise_pass_cs", defines);
			}

			// Create AO term and Edges views.
			[[unlikely]] if (!g_uav["xe_gtao_denoise_pass1"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				Com_ptr<ID3D11Texture2D> tex;
				ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, g_uav["xe_gtao_denoise_pass1"_h].put()), >= 0);
				ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["xe_gtao_denoise_pass1"_h].put()), >= 0);
			}

			// Bindings.
			ctx->CSSetUnorderedAccessViews(0, 1, &g_uav["xe_gtao_denoise_pass1"_h], nullptr);
			ctx->CSSetShader(g_cs["xe_gtao_denoise_pass1"_h].get(), nullptr, 0);
			ctx->CSSetShaderResources(0, 1, &g_srv["xe_gtao_main_pass"_h]);

			ctx->Dispatch((g_swapchain_width + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (g_swapchain_height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,1);

			//

			// XeGTAODenoisePass2 pass
			//

			// Create CS.
			[[unlikely]] if (!g_cs["xe_gtao_denoise_pass2"_h]) {
				static constexpr D3D_SHADER_MACRO defines[] = {
					{ "XE_GTAO_FINAL_APPLY", "1" },
					{ nullptr, nullptr }
				};
				create_compute_shader(device.get(), g_cs["xe_gtao_denoise_pass2"_h].put(), L"XeGTAO_impl.hlsl", "denoise_pass_cs", defines);
			}

			// Bindings.
			ctx->CSSetUnorderedAccessViews(0, 1, &uav_normal, nullptr);
			ctx->CSSetShader(g_cs["xe_gtao_denoise_pass2"_h].get(), nullptr, 0);
			ctx->CSSetShaderResources(0, 1, &g_srv["xe_gtao_denoise_pass1"_h]);

			ctx->Dispatch((g_swapchain_width + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (g_swapchain_height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);

			// Unbind UAV.
			static constexpr ID3D11UnorderedAccessView* uav_null = nullptr;
			ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);

			//

			#if DEV && SHOW_AO
			g_srv_ao = srv_normal;
			#endif

			g_has_drawn_xe_gtao = true;
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_prefilter_0xB357A376_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_prefilter_0xB357A376_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("ps_bloom_prefilter_0xB357A376: The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D11_FILTER_ANISOTROPIC
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(0, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_ANISOTROPIC || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("ps_bloom_prefilter_0xB357A376: The expected sampler in the slot 0 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup blend.
		Com_ptr<ID3D11BlendState> blend_original;
		float blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Bloom
		//

		// Create MIPs and views.
		////

		// The original prefilter should match this.
		const UINT y_mip0_width = g_swapchain_width / 2;
		const UINT y_mip0_height = g_swapchain_height / 2;

		// Create Y MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_y[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = y_mip0_width;
			tex_desc.Height = y_mip0_height;
			tex_desc.MipLevels = g_bloom_nmips;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			
			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = tex_desc.Format;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
			
			for (int i = 0; i < g_bloom_nmips; ++i) {
				rtv_desc.Texture2D.MipSlice = i;
				ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_bloom_mips_y[i]), >= 0);
				srv_desc.Texture2D.MostDetailedMip = i;
				ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_bloom_mips_y[i]), >= 0);
			}
		}

		const UINT x_mip0_width = g_swapchain_width / 2;
		const UINT x_mip0_height = g_swapchain_height;

		// Create X MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_x[1]) {
			// Create X MIP0 and views.
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = x_mip0_width;
			tex_desc.Height = x_mip0_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			
			// We don't need/use X MIP0 at all!
			// TODO: Refactor this and the later code.
			//ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			//ensure(device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[0]), >= 0);
			//ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[0]), >= 0);

			// Create rest of X MIPs and views.
			for (UINT i = 1; i < g_bloom_nmips; ++i) {
				tex_desc.Width = std::max(1u, x_mip0_width >> i);
				tex_desc.Height = std::max(1u, x_mip0_height >> i);
				ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[i]), >= 0);
				ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[i]), >= 0);
			}
		}

		////

		// Prefilter + downsample pass
		////

		// Create PS.
		[[unlikely]] if (!g_ps["bloom_prefilter_0xB357A376"_h]) {
			create_pixel_shader(device.get(), g_ps["bloom_prefilter_0xB357A376"_h].put(), L"BloomPrefilter_0xB357A376_ps.hlsl");
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[0], nullptr);
		ctx->PSSetShader(g_ps["bloom_prefilter_0xB357A376"_h].get(), nullptr, 0);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		////

		// Downsample passes
		////

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["bloom_downsample"_h]) {
			create_pixel_shader(device.get(), g_ps["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create CB.
		[[unlikely]] if (!g_cb["cb"_h]) {
			create_constant_buffer(device.get(), sizeof(g_cb_data), g_cb["cb"_h].put());
		}

		// Create vieewports.
		D3D11_VIEWPORT viewport_x = {};
		std::vector<D3D11_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Bindings.
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["bloom_downsample"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(13, 1, &g_cb["cb"_h]);

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_data.axis = float2(1.0f, 0.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			g_cb_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i - 1]);
			ctx->RSSetViewports(1, &viewport_x);

			// Draw X pass.
			ctx->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);
			ctx->RSSetViewports(1, &viewports_y[i]);

			// Draw Y pass.
			ctx->Draw(3, 0);
		}

		////

		// Upsample passes
		////

		// Create PS.
		[[unlikely]] if (!g_ps["bloom_upsample"_h]) {
			create_pixel_shader(device.get(), g_ps["bloom_upsample"_h].put(), L"Bloom_impl.hlsl", "upsample_ps");
		}

		// Create blend.
		[[unlikely]] if (!g_blend["bloom"_h]) {
			CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
			ensure(device->CreateBlendState(&desc, g_blend["bloom"_h].put()), >= 0);
		}

		// Bindings.
		ctx->PSSetShader(g_ps["bloom_upsample"_h].get(), nullptr, 0);

		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// If both dst and src are D3D11_BLEND_BLEND_FACTOR,
			// factor of 0.5 will be enegrgy preserving.
			static constexpr FLOAT blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i - 1], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i]);
			ctx->RSSetViewports(1, &viewports_y[i - 1]);
			ctx->OMSetBlendState(g_blend["bloom"_h].get(), blend_factor, UINT_MAX);

			ctx->Draw(3, 0);
		}

		//

		// Restore.
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);
		ctx->VSSetShader(vs_original.get(), nullptr, 0);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x807FB946_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x807FB946_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x81EC9518_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x81EC9518_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xEA5892CE_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xEA5892CE_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_add_0x24314FFA_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_add_0x24314FFA_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("ps_bloom_add_0x24314FFA: The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D11_FILTER_ANISOTROPIC
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(0, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_ANISOTROPIC || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("ps_bloom_add_0x24314FFA: The expected sampler in the slot 0 wasn't what we expected it to be!");
		}
		#endif // DEV

		// smps 0 and 1 should be anistropyic clamp.

		// SMAA
		//

		// Backup RTV.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Backup smp.
		Com_ptr<ID3D11SamplerState> smp_original;
		ctx->PSGetSamplers(1, 1, smp_original.put());

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// SRV1 should be the scene in sRGB light,
		// the resource should be backbuffer.
		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		ctx->PSGetShaderResources(1, 1, srv_scene.put());

		// Linearize pass
		////

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["linearize"_h]) {
			create_pixel_shader(device.get(), g_ps["linearize"_h].put(), L"Linearize_ps.hlsl");
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["scene_linearized"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["scene_linearized"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["scene_linearized"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["scene_linearized"_h], nullptr);
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["linearize"_h].get(), nullptr, 0);

		ctx->Draw(3, 0);

		////

		// SMAAEdgeDetection pass
		////

		// Create VS.
		[[unlikely]] if (!g_vs["smaa_edge_detection"_h]) {
			g_smaa_rt_metrics.set(g_swapchain_width, g_swapchain_height);
			create_vertex_shader(device.get(), g_vs["smaa_edge_detection"_h].put(), L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps["smaa_edge_detection"_h]) {
			create_pixel_shader(device.get(), g_ps["smaa_edge_detection"_h].put(), L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create point sampler.
		[[unlikely]] if (!g_smp["point_clamp"_h]) {
			create_sampler_point_clamp(device.get(), g_smp["point_clamp"_h].put());
		}

		// Create DS.
		[[unlikely]] if (!g_ds["smaa_disable_depth_replace_stencil"_h]) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, g_ds["smaa_disable_depth_replace_stencil"_h].put()), >= 0);
		}

		// Create DSV.
		[[unlikely]] if (!g_dsv["smaa_dsv"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateDepthStencilView(tex.get(), nullptr, g_dsv["smaa_dsv"_h].put()), >= 0);
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["smaa_edge_detection"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["smaa_edge_detection"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["smaa_edge_detection"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds["smaa_disable_depth_replace_stencil"_h].get(), 1);
		ctx->OMSetRenderTargets(1, &g_rtv["smaa_edge_detection"_h], g_dsv["smaa_dsv"_h].get());
		ctx->VSSetShader(g_vs["smaa_edge_detection"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["smaa_edge_detection"_h].get(), nullptr, 0);
		ctx->PSSetSamplers(1, 1, &g_smp["point_clamp"_h]);
		const std::array ps_srvs_smaa_edge_detection = { srv_scene.get(), g_srv["depth"_h].get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_edge_detection.size(), ps_srvs_smaa_edge_detection.data());

		static constexpr float smaa_clear_color[4] = {};
		ctx->ClearRenderTargetView(g_rtv["smaa_edge_detection"_h].get(), smaa_clear_color);
		ctx->ClearDepthStencilView(g_dsv["smaa_dsv"_h].get(), D3D11_CLEAR_STENCIL, 1.0f, 0);
		ctx->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		////

		// Create VS.
		[[unlikely]] if (!g_vs["smaa_blending_weight_calculation"_h]) {
			create_vertex_shader(device.get(), g_vs["smaa_blending_weight_calculation"_h].put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps["smaa_blending_weight_calculation"_h]) {
			create_pixel_shader(device.get(), g_ps["smaa_blending_weight_calculation"_h].put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!g_srv["smaa_area_tex"_h]) {
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = AREATEX_WIDTH;
			desc.Height = AREATEX_HEIGHT;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = areaTexBytes;
			subresource_data.SysMemPitch = AREATEX_PITCH;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["smaa_area_tex"_h].put()), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!g_srv["smaa_search_tex"_h]) {
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = SEARCHTEX_WIDTH;
			desc.Height = SEARCHTEX_HEIGHT;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = searchTexBytes;
			subresource_data.SysMemPitch = SEARCHTEX_PITCH;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["smaa_search_tex"_h].put()), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_ds["smaa_disable_depth_use_stencil"_h]) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, g_ds["smaa_disable_depth_use_stencil"_h].put()), >= 0);
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["smaa_blending_weight_calculation"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["smaa_blending_weight_calculation"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["smaa_blending_weight_calculation"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds["smaa_disable_depth_use_stencil"_h].get(), 1);
		ctx->OMSetRenderTargets(1, &g_rtv["smaa_blending_weight_calculation"_h], g_dsv["smaa_dsv"_h].get());
		ctx->VSSetShader(g_vs["smaa_blending_weight_calculation"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["smaa_blending_weight_calculation"_h].get(), nullptr, 0);
		const std::array ps_srvs_smaa_blending_weight_calculation = { g_srv["smaa_edge_detection"_h].get(), g_srv["smaa_area_tex"_h].get(), g_srv["smaa_search_tex"_h].get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_blending_weight_calculation.size(), ps_srvs_smaa_blending_weight_calculation.data());

		ctx->ClearRenderTargetView(g_rtv["smaa_blending_weight_calculation"_h].get(), smaa_clear_color);
		ctx->Draw(3, 0);

		////

		// SMAANeighborhoodBlending pass
		////

		// Create VS.
		[[unlikely]] if (!g_vs["smaa_neighborhood_blending"_h]) {
			create_vertex_shader(device.get(), g_vs["smaa_neighborhood_blending"_h].put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps["smaa_neighborhood_blending"_h]) {
			create_pixel_shader(device.get(), g_ps["smaa_neighborhood_blending"_h].put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["smaa_neighborhood_blending"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["smaa_neighborhood_blending"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["smaa_neighborhood_blending"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["smaa_neighborhood_blending"_h], nullptr);
		ctx->VSSetShader(g_vs["smaa_neighborhood_blending"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["smaa_neighborhood_blending"_h].get(), nullptr, 0);
		const std::array ps_srvs_neighborhood_blending = { g_srv["scene_linearized"_h].get(), g_srv["smaa_blending_weight_calculation"_h].get() };
		ctx->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

		ctx->Draw(3, 0);

		////
		
		// Restore.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->PSSetSamplers(1, 1, &smp_original);

		//

		// Create PS.
		[[unlikely]] if (!g_ps["bloom_add_0x24314FFA"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["bloom_add_0x24314FFA"_h].put(), L"BloomAdd_0x24314FFA_ps.hlsl", "main", defines);
		}

		// Bindings.
		#if DEV && SHOW_AO
		ctx->PSSetShader(g_ps["bloom_add_0x24314FFA"_h].get(), nullptr, 0);
		ID3D11ShaderResourceView* srv_null = nullptr;
		ctx->PSSetShaderResources(0, 1, &srv_null);
		ctx->PSSetShaderResources(1, 1, &g_srv_ao);
		#else
		ctx->PSSetShader(g_ps["bloom_add_0x24314FFA"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[0]);
		ctx->PSSetShaderResources(1, 1, &g_srv["smaa_neighborhood_blending"_h]);
		#endif

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_color_grading_and_vignette_0xBFF40A4D_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_color_grading_and_vignette_0xBFF40A4D_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("ps_color_grading_and_vignette_0xBFF40A4D: The expected primitive topology wasn't what we expected it to be!");
		}
		#endif // DEV
		
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// ColorGradingAndVignette pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps["color_grading_and_vignette_0xBFF40A4D"_h]) {
			create_pixel_shader(device.get(), g_ps["color_grading_and_vignette_0xBFF40A4D"_h].put(), L"ColorGradingAndVignette_0xBFF40A4D_ps.hlsl");
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv["color_grading_and_vignette_0xBFF40A4D"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["color_grading_and_vignette_0xBFF40A4D"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["color_grading_and_vignette_0xBFF40A4D"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["color_grading_and_vignette_0xBFF40A4D"_h], nullptr);
		ctx->PSSetShader(g_ps["color_grading_and_vignette_0xBFF40A4D"_h].get(), nullptr, 0);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

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
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["amd_ffx_cas"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["amd_ffx_cas"_h], nullptr);
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["amd_ffx_cas"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv["color_grading_and_vignette_0xBFF40A4D"_h]);

		ctx->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

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
			std::array<D3D11_SUBRESOURCE_DATA, BLUE_NOISE_TEX_ARRAY_SIZE> subresource_data = {};
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
			g_cb_data.tex_noise_index += 1.0f;
			if (g_cb_data.tex_noise_index >= (float)BLUE_NOISE_TEX_ARRAY_SIZE) {
				g_cb_data.tex_noise_index = 0.0f;
			}
			update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_ps["amd_ffx_lfga"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(13, 1, &g_cb["cb"_h]);
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
	auto hr = cs->GetPrivateData(g_cs_ssao_denoise_0x54A9A847_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ssao_denoise_0x54A9A847_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ssao_denoise_0x8A03353C_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ssao_denoise_0x8A03353C_hash) {
		return true;
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
				case g_ps_copy_0xB8813A2F_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_copy_0xB8813A2F_guid, sizeof(g_ps_copy_0xB8813A2F_hash), &g_ps_copy_0xB8813A2F_hash), >= 0);
					break;
				case g_ps_ssao_0x7A054979_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_0x7A054979_guid, sizeof(g_ps_ssao_0x7A054979_hash), &g_ps_ssao_0x7A054979_hash), >= 0);
					break;
				case g_ps_bloom_prefilter_0xB357A376_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_prefilter_0xB357A376_guid, sizeof(g_ps_bloom_prefilter_0xB357A376_hash), &g_ps_bloom_prefilter_0xB357A376_hash), >= 0);
					break;
				case g_ps_bloom_add_0x24314FFA_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_add_0x24314FFA_guid, sizeof(g_ps_bloom_add_0x24314FFA_hash), &g_ps_bloom_add_0x24314FFA_hash), >= 0);
					break;
				case g_ps_bloom_0x807FB946_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x807FB946_guid, sizeof(g_ps_bloom_0x807FB946_hash), &g_ps_bloom_0x807FB946_hash), >= 0);
					break;
				case g_ps_bloom_0x81EC9518_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x81EC9518_guid, sizeof(g_ps_bloom_0x81EC9518_hash), &g_ps_bloom_0x81EC9518_hash), >= 0);
					break;
				case g_ps_bloom_0xEA5892CE_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xEA5892CE_guid, sizeof(g_ps_bloom_0xEA5892CE_hash), &g_ps_bloom_0xEA5892CE_hash), >= 0);
					break;
				case g_ps_color_grading_and_vignette_0xBFF40A4D_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_color_grading_and_vignette_0xBFF40A4D_guid, sizeof(g_ps_color_grading_and_vignette_0xBFF40A4D_hash), &g_ps_color_grading_and_vignette_0xBFF40A4D_hash), >= 0);
					break;
			}
		}
		else if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_ssao_denoise_0x54A9A847_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ssao_denoise_0x54A9A847_guid, sizeof(g_cs_ssao_denoise_0x54A9A847_hash), &g_cs_ssao_denoise_0x54A9A847_hash), >= 0);
					break;
				case g_cs_ssao_denoise_0x8A03353C_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ssao_denoise_0x8A03353C_guid, sizeof(g_cs_ssao_denoise_0x8A03353C_hash), &g_cs_ssao_denoise_0x8A03353C_hash), >= 0);
					break;
			}
		}
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_unorm) {
			desc.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
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

	// The game uses swapchain backbuffer as any other resource, on top of that uses alpha,
	// so we can't reliabley upgrade it.

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

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Reset reolution dependent resources.
	g_vs["smaa_edge_detection"_h].reset();
	g_ps["smaa_edge_detection"_h].reset();
	g_vs["smaa_blending_weight_calculation"_h].reset();
	g_ps["smaa_blending_weight_calculation"_h].reset();
	g_vs["smaa_neighborhood_blending"_h].reset();
	g_ps["smaa_neighborhood_blending"_h].reset();
	g_rtv.clear();
	g_uav.clear();
	g_srv.clear();
	g_dsv.clear();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(device1.put());
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void on_destroy_device(reshade::api::device *device)
{
	g_rtv.clear();
	g_uav.clear();
	g_srv.clear();
	g_vs.clear();
	g_ps.clear();
	g_cs.clear();
	g_smp.clear();
	g_cb.clear();
	g_blend.clear();
	g_dsv.clear();
	g_ds.clear();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "DeusExHRGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	#if DEV
	if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1, 10)) {
		release_com_array(g_rtv_bloom_mips_y);
		release_com_array(g_srv_bloom_mips_y);
		release_com_array(g_rtv_bloom_mips_x);
		release_com_array(g_srv_bloom_mips_x);
		g_rtv_bloom_mips_y.resize(g_bloom_nmips);
		g_srv_bloom_mips_y.resize(g_bloom_nmips);
		g_rtv_bloom_mips_x.resize(g_bloom_nmips);
		g_srv_bloom_mips_x.resize(g_bloom_nmips);
		g_bloom_sigmas.resize(g_bloom_nmips);
	}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	}
	if (ImGui::Button("Dev button")) {
	}
	ImGui::Spacing();
	#endif

	if (ImGui::SliderFloat("XeGTAO radius", &g_xe_gtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
		g_cs["xe_gtao_prefilter_depths16x16"_h].reset();
		g_cs["xe_gtao_main_pass"_h].reset();
	}
	static constexpr std::array xe_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO quality", &g_xe_gtao_quality, xe_gtao_quality_items.data(), xe_gtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
		g_cs["xe_gtao_main_pass"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps["bloom_add_0x24314FFA"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps["amd_ffx_cas"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "TRC", g_trc);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "Gamma", g_gamma);
		g_ps["amd_ffx_lfga"_h].reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "DeusExHRGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "DeusExHRGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "DeusExHRGraphicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/DeusExHRGraphicalUpgrade";

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

			// Bloom.
			g_bloom_nmips = 6;
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 0.0f; // Unused.
			g_bloom_sigmas[1] = 2.0f;
			g_bloom_sigmas[2] = 2.0f;
			g_bloom_sigmas[3] = 2.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
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
			break;
	}
	return TRUE;
}