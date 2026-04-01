#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Common.h"
#include "Helpers.h"
#include "HLSLTypes.h"
#include "TRC.h"

struct alignas(16) CB_data
{
   float2 src_size;
   float2 inv_src_size;
   float2 axis;
   float sigma;
   float padding;
};

// Shader hooks.
//

// HBAO - main pass
constexpr uint32_t g_ps_hbao_0x32C4EB43_hash = 0x32C4EB43;
constexpr GUID g_ps_hbao_0x32C4EB43_guid = { 0x32abcca1, 0x34f6, 0x4f6e, { 0xad, 0x25, 0x77, 0x87, 0x5d, 0x71, 0xf6, 0x64 } };

constexpr uint32_t g_ps_hbao_0xF68A9768_hash = 0xF68A9768;
constexpr GUID g_ps_hbao_0xF68A9768_guid = { 0xebc2a412, 0xa17f, 0x43c8, { 0xae, 0x44, 0xcb, 0x39, 0x4, 0x89, 0xbc, 0x4f } };

constexpr uint32_t g_ps_hbao_0xEE6C036E_hash = 0xEE6C036E;
constexpr GUID g_ps_hbao_0xEE6C036E_guid = { 0xc62037e5, 0x5d0e, 0x48f3, { 0xb0, 0xd7, 0x39, 0x71, 0xd6, 0x3e, 0xc4, 0x65 } };

constexpr uint32_t g_ps_hbao_0x7666BE29_hash = 0x7666BE29;
constexpr GUID g_ps_hbao_0x7666BE29_guid = { 0x5ff9f378, 0xd8ad, 0x4ca8, { 0x81, 0x40, 0xf3, 0x94, 0xfc, 0xca, 0x30, 0xdd } };

// HBAO - denoise pass 2 and apply (the last HBAO pass).
constexpr uint32_t g_ps_hbao_0xF4EC5E4C_hash = 0xF4EC5E4C;
constexpr GUID g_ps_hbao_0xF4EC5E4C_guid = { 0x82ded408, 0x1c15, 0x4023, { 0xa7, 0xf1, 0xa5, 0xc9, 0x0, 0xe1, 0x15, 0x49 } };

constexpr uint32_t g_ps_tonemap_0x2AC9C1EF_hash = 0x2AC9C1EF;
constexpr GUID g_ps_tonemap_0x2AC9C1EF_guid = { 0xcd66e88e, 0xfc24, 0x4f1c, { 0xb9, 0x4a, 0xcc, 0x32, 0xa7, 0xd1, 0xba, 0x37 } };

//

static int g_swapchain_width;
static int g_swapchain_height;
static float g_amd_ffx_rcas_sharpness = 0.4f;
static CB_data g_cb_data;
static bool g_force_vsync_off = true;
static TRC g_trc = TRC_GAMMA;
static float g_gamma = 2.2f;
static bool g_disable_lens_flare;
static bool g_disable_graphical_upgrade_shaders;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static float g_xegtao_fov_y = 55.0f; // in degrees
static float g_xe_gtao_radius = 0.6f;
static int g_xe_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

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
static std::array<ID3D11UnorderedAccessView*, XE_GTAO_DEPTH_MIP_LEVELS> g_uav_xe_gtao_prefilter_depths16x16;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_x;

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	// The game is crashing offten on level load with the error DXGI_ERROR_DEVICE_REMOVED and DXGI_ERROR_DEVICE_HUNG as the reason.
	[[unlikely]] if (g_disable_graphical_upgrade_shaders) {
		return false;
	}

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(ps.put(), nullptr, nullptr);
	if (!ps) {
		return false;
	}

	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_hbao_0x32C4EB43_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_hbao_0x32C4EB43_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_hbao_0xF68A9768_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_hbao_0xF68A9768_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_hbao_0xEE6C036E_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_hbao_0xEE6C036E_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_hbao_0x7666BE29_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_hbao_0x7666BE29_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_hbao_0xF4EC5E4C_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_hbao_0xF4EC5E4C_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Already linearized depth should be bound to slot 1.
		Com_ptr<ID3D11ShaderResourceView> srv_linearized_depth;
		ctx->PSGetShaderResources(1, 1, srv_linearized_depth.put());

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
		ctx->CSSetShaderResources(0, 1, &srv_linearized_depth);
		ctx->CSSetSamplers(0, 1, &g_smp["point_clamp"_h]);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, g_uav_xe_gtao_prefilter_depths16x16.size()> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//

		// XeGTAOMainPass pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["xe_gtao_main_pass"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const float tan_half_fov_y = std::tan(g_xegtao_fov_y * to_rad_mul<float> * 0.5f);
			const float tan_half_fov_x = tan_half_fov_y * (float)g_swapchain_width / (float)g_swapchain_height;
			const std::string ndc_to_view_mul_str = std::format("float2({},{})", tan_half_fov_x * 2.0f, tan_half_fov_y * -2.0f);
			const std::string ndc_to_view_add_str = std::format("float2({},{})", -tan_half_fov_x, tan_half_fov_y);
			const std::string xe_gtao_quality_val = std::to_string(g_xe_gtao_quality);
			const std::string effect_radius_val = std::to_string(g_xe_gtao_radius);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "NDC_TO_VIEW_MUL", ndc_to_view_mul_str.c_str() },
				{ "NDC_TO_VIEW_ADD", ndc_to_view_add_str.c_str() },
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
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav[hash_name("xe_gtao_main_pass")], nullptr);
		ctx->CSSetShader(g_cs["xe_gtao_main_pass"_h].get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &g_srv["xe_gtao_prefilter_depths16x16"_h]);

		ctx->Dispatch((g_swapchain_width + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, (g_swapchain_height + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, 1);

		//

		// Doing 2 XeGTAODenoisePass passes correspond to "Denoising level: Medium" from the XeGTAO demo.

		// XeGTAODenoisePass pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["xe_gtao_denoise_pass1"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "XE_GTAO_FINAL_APPLY", "0" },
				{ nullptr, nullptr }
			};
			create_compute_shader(device.get(), g_cs["xe_gtao_denoise_pass1"_h].put(), L"XeGTAO_impl.hlsl", "denoise_pass_cs", defines);
		}

		// Create UAV.
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

		ctx->Dispatch((g_swapchain_width + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2), (g_swapchain_height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,1);

		//

		// XeGTAODenoisePass2 pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs["xe_gtao_denoise_pass2"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "XE_GTAO_FINAL_APPLY", "1" },
				{ nullptr, nullptr }
			};
			create_compute_shader(device.get(), g_cs["xe_gtao_denoise_pass2"_h].put(), L"XeGTAO_impl.hlsl", "denoise_pass_cs", defines);
		}

		// Create UAV.
		[[unlikely]] if (!g_uav["xe_gtao_denoise_pass2"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, g_uav["xe_gtao_denoise_pass2"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["xe_gtao_denoise_pass2"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_uav["xe_gtao_denoise_pass2"_h], nullptr);
		ctx->CSSetShader(g_cs["xe_gtao_denoise_pass2"_h].get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &g_srv["xe_gtao_denoise_pass1"_h]);

		ctx->Dispatch((g_swapchain_width + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2), (g_swapchain_height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);

		// Unbind UAV.
		static constexpr ID3D11UnorderedAccessView* uav_null = {};
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);

		//

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("ps_hbao_0xF4EC5E4C: The expected primitive topology wasn't what we expected it to be!");
		}
		#endif

		// XeGTAOCopy pass
		//
		// We rely on the existing blend to apply the AO.
		//

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["xe_gtao_copy"_h]) {
			create_pixel_shader(device.get(), g_ps["xe_gtao_copy"_h].put(), L"XeGTAO_impl.hlsl", "copy_ps");
		}

		// Bindings.
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["xe_gtao_copy"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv["xe_gtao_denoise_pass2"_h]);

		ctx->Draw(3, 0);

		//

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x2AC9C1EF_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x2AC9C1EF_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("ps_tonemap_0x2AC9C1EF: The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_LINEAR
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(1, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("ps_tonemap_0x2AC9C1EF: The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup viewports.
		UINT nviewports_original;
		ctx->RSGetViewports(&nviewports_original, nullptr);
		std::vector<D3D11_VIEWPORT> viewports_original(nviewports_original);
		ctx->RSGetViewports(&nviewports_original, viewports_original.data());

		// Backup blend.
		Com_ptr<ID3D11BlendState> blend_original;
		float blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		Com_ptr<ID3D11RenderTargetView> rtv_original;
		Com_ptr<ID3D11DepthStencilView> dsv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());

		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		ctx->PSGetShaderResources(0, 1, srv_scene.put());
		
		// Bloom
		//

		// Create MIPs and views.
		////

		const UINT y_mip0_width = g_swapchain_width >> 1;
		const UINT y_mip0_height = g_swapchain_height >> 1;

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

		const UINT x_mip0_width = g_swapchain_width >> 1;
		const UINT x_mip0_height = g_swapchain_height;

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
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[0]), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[0]), >= 0);

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

		// Create CB.
		[[unlikely]] if (!g_cb[hash_name("cb")]) {
			create_constant_buffer(device.get(), sizeof(g_cb_data), g_cb["cb"_h].put());
		}

		// Prefilter + downsample pass
		////

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("bloom_downsample")]) {
			create_pixel_shader(device.get(), g_ps["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("bloom_prefilter")]) {
			create_pixel_shader(device.get(), g_ps["bloom_prefilter"_h].put(), L"Bloom_impl.hlsl", "prefilter_ps");
		}

		D3D11_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(g_swapchain_width, g_swapchain_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[0], nullptr);
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["bloom_downsample"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(13, 1, &g_cb["cb"_h]);
		ctx->PSSetShaderResources(0, 1, &srv_scene);
		ctx->RSSetViewports(1, &viewport_x);

		// Draw X pass.
		ctx->Draw(3, 0);

		std::vector<D3D11_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(x_mip0_width, x_mip0_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(0.0f, 1.0f);
		update_constant_buffer(ctx, g_cb["cb"_h].get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[0], nullptr);
		ctx->PSSetShader(g_ps["bloom_prefilter"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);
		ctx->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		ctx->Draw(3, 0);

		////

		// Downsample passes
		////

		// Bindings.
		ctx->PSSetShader(g_ps["bloom_downsample"_h].get(), nullptr, 0);

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

		// Toeneamp_0x2AC9C1EF pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps["tonemap_0x2AC9C1EF"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string disable_lens_flare_str = std::to_string((int)g_disable_lens_flare);
			D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ "DISABLE_LENS_FLARE" , disable_lens_flare_str.c_str() },
				{ nullptr , nullptr }
			};
			create_pixel_shader(device.get(), g_ps["tonemap_0x2AC9C1EF"_h].put(), L"Tonemap_0x2AC9C1EF_ps.hlsl", "main", defines);
		}

		// Create RTV.
		[[unlikely]] if (!g_rtv["tonemap_0x2AC9C1EF"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv["tonemap_0x2AC9C1EF"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv["tonemap_0x2AC9C1EF"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv["tonemap_0x2AC9C1EF"_h], dsv_original.get());
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->RSSetViewports(nviewports_original, viewports_original.data());
		ctx->PSSetShader(g_ps["tonemap_0x2AC9C1EF"_h].get(), nullptr, 0);
		const std::array srvs_tonemap_0x2AC9C1EF = { srv_scene.get(), g_srv_bloom_mips_y[0] };
		ctx->PSSetShaderResources(0, srvs_tonemap_0x2AC9C1EF.size(), srvs_tonemap_0x2AC9C1EF.data());
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		// AMD FFX RCAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_vs["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps["amd_ffx_rcas"_h]) {
			const std::string amd_ffx_rcas_sharpness_str = std::to_string(g_amd_ffx_rcas_sharpness);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_rcas_sharpness_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps["amd_ffx_rcas"_h].put(), L"AMD_FFX_RCAS_ps.hlsl", "main", defines);
		}

		// Bindings.
		////

		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->VSSetShader(g_vs["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_ps["amd_ffx_rcas"_h].get(), nullptr, 0);

		#if DEV && SHOW_AO
		ctx->PSSetShaderResources(0, 1, &g_srv["xe_gtao_denoise_pass2"_h]);
		#else
		ctx->PSSetShaderResources(0, 1, &g_srv["tonemap_0x2AC9C1EF"_h]);
		#endif

		////

		ctx->Draw(3, 0);

		//

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
				case g_ps_hbao_0x32C4EB43_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_hbao_0x32C4EB43_guid, sizeof(g_ps_hbao_0x32C4EB43_hash), &g_ps_hbao_0x32C4EB43_hash), >= 0);
					break;
				case g_ps_hbao_0xF68A9768_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_hbao_0xF68A9768_guid, sizeof(g_ps_hbao_0xF68A9768_hash), &g_ps_hbao_0xF68A9768_hash), >= 0);
					break;
				case g_ps_hbao_0xEE6C036E_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_hbao_0xEE6C036E_guid, sizeof(g_ps_hbao_0xEE6C036E_hash), &g_ps_hbao_0xEE6C036E_hash), >= 0);
					break;
				case g_ps_hbao_0x7666BE29_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_hbao_0x7666BE29_guid, sizeof(g_ps_hbao_0x7666BE29_hash), &g_ps_hbao_0x7666BE29_hash), >= 0);
					break;
				case g_ps_hbao_0xF4EC5E4C_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_hbao_0xF4EC5E4C_guid, sizeof(g_ps_hbao_0xF4EC5E4C_hash), &g_ps_hbao_0xF4EC5E4C_hash), >= 0);
					break;
				case g_ps_tonemap_0x2AC9C1EF_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x2AC9C1EF_guid, sizeof(g_ps_tonemap_0x2AC9C1EF_hash), &g_ps_tonemap_0x2AC9C1EF_hash), >= 0);
					break;
			}
		}
	}
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	// It looks like the game is ignoring the in game anistropic filtering option
	// and it's setting it to 4 always.
	desc.max_anisotropy = 16.0f;

	return true;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		// This seems to be the best place to force the game into borderless window.
		auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
		DXGI_SWAP_CHAIN_DESC desc;
		native_swapchain->GetDesc(&desc);
		SetWindowLongW(desc.OutputWindow, GWL_STYLE, WS_POPUP);
		SetWindowPos(desc.OutputWindow, HWND_TOP, 0, 0, desc.BufferDesc.Width, desc.BufferDesc.Height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// The game is rendering much more than UI to backbuffer. Is alpha ever used?
	//desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	
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

	// Reset resolution dependent resources.
	g_cs["xe_gtao_prefilter_depths16x16"_h].reset();
	g_cs["xe_gtao_main_pass"_h].reset();
	g_cs["xe_gtao_denoise_pass1"_h].reset();
	g_cs["xe_gtao_denoise_pass2"_h].reset();
	g_rtv.clear();
	g_uav.clear();
	g_srv.clear();
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
	g_ps.clear();
	g_vs.clear();
	g_cs.clear();
	g_smp.clear();
	g_cb.clear();
	g_blend.clear();
	release_com_array(g_uav_xe_gtao_prefilter_depths16x16);
	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableGraphicalUpgradeShaders", g_disable_graphical_upgrade_shaders)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableGraphicalUpgradeShaders", g_disable_graphical_upgrade_shaders);
	}
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

	ImGui::InputFloat("XeGTAO FOV Y", &g_xegtao_fov_y);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_xegtao_fov_y = std::clamp(g_xegtao_fov_y, 0.0f, 180.0f);
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
		g_cs["xe_gtao_main_pass"_h].reset();
	}
	if (ImGui::SliderFloat("XeGTAO radius", &g_xe_gtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
		g_cs["xe_gtao_prefilter_depths16x16"_h].reset();
		g_cs["xe_gtao_main_pass"_h].reset();
	}
	static constexpr std::array xe_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO quality", &g_xe_gtao_quality, xe_gtao_quality_items.data(), xe_gtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
		g_cs["xe_gtao_main_pass"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps["tonemap_0x2AC9C1EF"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_rcas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness);
		g_ps["amd_ffx_rcas"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable lens flare", &g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
		g_ps["tonemap_0x2AC9C1EF"_h].reset();
	}

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "TRC", g_trc);
		g_ps["amd_ffx_rcas"_h].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Gamma", g_gamma);
		g_ps["amd_ffx_rcas"_h].reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Disable GraphicalUpgrade shaders", &g_disable_graphical_upgrade_shaders)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableGraphicalUpgradeShaders", g_disable_graphical_upgrade_shaders);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("If the game is crashing on level load try enabeling this option before the level load.\nAfter the level successfuly loads you can disable this option to re-enable GraphicalUpgrade shaders.");
	}
}

extern "C" __declspec(dllexport) const char* NAME = "BFBC2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "BFBC2GraphicalUpgrade v2.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BFBC2GraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_graphical_upgrade_path();
			read_config();

			// Bloom.
			g_bloom_nmips = 6;
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 1.5f;
			g_bloom_sigmas[1] = 2.0f;
			g_bloom_sigmas[2] = 2.0f;
			g_bloom_sigmas[3] = 2.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
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
