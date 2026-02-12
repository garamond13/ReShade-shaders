#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Helpers.h"
#include "HLSLTypes.h"
#include "AreaTex.h"
#include "SearchTex.h"

struct alignas(16) CB_data
{
   float2 src_size;
   float2 inv_src_size;
   float2 axis;
   float sigma;
};

// We need to pass this to all SMAA shaders on resolution change.
class SMAA_rt_metrics
{
public:

	void set(float width, float height)
	{
		val_str = std::format("float4({},{},{},{})", 1.0f / width, 1.0f / height, width, height);
		defines[0] = { "SMAA_RT_METRICS", val_str.c_str() };
		defines[1] = { nullptr, nullptr };
	}

	const D3D_SHADER_MACRO* get() const
	{
		return defines;
	}

private:

	std::string val_str;
	D3D_SHADER_MACRO defines[2] = {};
};

// Shader hooks.
//

constexpr uint32_t g_ps_depth_copy_0x496E549B_hash = 0x496E549B;
constexpr GUID g_ps_depth_copy_0x496E549B_guid = { 0xfeb12521, 0xc4fa, 0x426a, { 0x8c, 0x3, 0x3e, 0xa6, 0x60, 0x6f, 0x60, 0x95 } };

constexpr uint32_t g_ps_tonemap_0x29D570D8_hash = 0x29D570D8;
constexpr GUID g_ps_tonemap_0x29D570D8_guid = { 0x892b84bd, 0x1a7, 0x442c, { 0x86, 0x60, 0xcb, 0x69, 0xf0, 0xc3, 0x2e, 0x68 } };

constexpr uint32_t g_ps_fxaa_0x27BD2A2E_hash = 0x27BD2A2E;
constexpr GUID g_ps_fxaa_0x27BD2A2E_guid = { 0xbedb4e98, 0x423b, 0x4d98, { 0x93, 0x6d, 0xc, 0x8a, 0xd8, 0x87, 0xde, 0x4f } };

constexpr uint32_t g_cs_ao_main_high_0x1E7B9941_hash = 0x1E7B9941;
constexpr GUID g_cs_ao_main_high_0x1E7B9941_guid = { 0xc8712845, 0x4961, 0x4b52, { 0x9d, 0x7a, 0xf2, 0x6e, 0xf0, 0x65, 0xd6, 0x2f } };

constexpr uint32_t g_cs_ao_main_ultra_0x348372D0_hash = 0x348372D0;
constexpr GUID g_cs_ao_main_ultra_0x348372D0_guid = { 0x86e294be, 0x82b8, 0x41af, { 0xa4, 0xea, 0xdb, 0xd6, 0x9d, 0x11, 0x36, 0xb6 } };

constexpr uint32_t g_cs_ao_denoise1_0xF6ED18D8_hash = 0xF6ED18D8;
constexpr GUID g_cs_ao_denoise1_0xF6ED18D8_guid = { 0xf2278831, 0xab7a, 0x4949, { 0xb8, 0x69, 0x68, 0xf2, 0xfb, 0xc7, 0xd, 0x88 } };

constexpr uint32_t g_cs_ao_denoise2_0xBA9A4DB1_hash = 0xBA9A4DB1;
constexpr GUID g_cs_ao_denoise2_0xBA9A4DB1_guid = { 0xe6858b28, 0x6e5f, 0x4485, { 0xbb, 0xee, 0xa7, 0x73, 0x87, 0x60, 0x4d, 0xe7 } };

constexpr uint32_t g_ps_lens_flare_0xE5427265_hash = 0xE5427265;
constexpr GUID g_ps_lens_flare_0xE5427265_guid = { 0xe92db518, 0x4b0d, 0x4b24, { 0x91, 0x65, 0x1b, 0x2e, 0xa7, 0x81, 0x96, 0x81 } };

constexpr uint32_t g_ps_bloom_prefilter_dof_0xC39285AF_hash = 0xC39285AF;
constexpr GUID g_ps_bloom_prefilter_dof_0xC39285AF_guid = { 0x5e59c86e, 0xc9f2, 0x47e5, { 0xab, 0x20, 0x1, 0x57, 0xcb, 0xf3, 0x20, 0x0 } };
constexpr uint32_t g_ps_bloom_prefilter_dof_0x6F4F1E8F_hash = 0x6F4F1E8F;
constexpr GUID g_ps_bloom_prefilter_dof_0x6F4F1E8F_guid = { 0x595508db, 0xa6e9, 0x4bb6, { 0x9c, 0xa2, 0x6e, 0x40, 0x8e, 0x8, 0xda, 0xdb } };
constexpr uint32_t g_ps_bloom_prefilter_dof_0x9113DE68_hash = 0x9113DE68;
constexpr GUID g_ps_bloom_prefilter_dof_0x9113DE68_guid = { 0xe9d94101, 0xf98d, 0x443d, { 0x98, 0xe5, 0x53, 0xd5, 0x8d, 0xa4, 0xe3, 0xe6 } };
constexpr uint32_t g_ps_bloom_prefilter_dof_0xAD03A911_hash = 0xAD03A911;
constexpr GUID g_ps_bloom_prefilter_dof_0xAD03A911_guid = { 0x1d600368, 0x3ffd, 0x4b37, { 0x93, 0xa, 0xad, 0x56, 0x8f, 0x8f, 0xc7, 0xf5 } };

constexpr uint32_t g_ps_starburst_and_lens_dirt_0x2AD953ED_hash = 0x2AD953ED;
constexpr GUID g_ps_starburst_and_lens_dirt_0x2AD953ED_guid = { 0xecdc207f, 0x2731, 0x48ea, { 0xb6, 0x48, 0x82, 0xac, 0xd5, 0xfa, 0xda, 0x60 } };

constexpr uint32_t g_ps_object_highlight_white_0x4D93743F_hash = 0x4D93743F;
constexpr GUID g_ps_object_highlight_white_0x4D93743F_guid = { 0xab06e6f6, 0x4a73, 0x490c, { 0x88, 0xd0, 0x73, 0xfe, 0xbe, 0xa7, 0x29, 0x43 } };
constexpr uint32_t g_ps_object_highlight_red_0x61D986B8_hash = 0x61D986B8;
constexpr GUID g_ps_object_highlight_red_0x61D986B8_guid = { 0xe180a8e2, 0x2bb, 0x48a1, { 0xab, 0xc8, 0xa3, 0xd8, 0x7a, 0x33, 0xdc, 0x4c } };

//

static uint32_t g_swapchain_width;
static uint32_t g_swapchain_height;
static CB_data g_cb_data;
static bool g_disable_lens_flare = true;
static bool g_disable_lens_dirt = true;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static float g_xe_gtao_radius = 0.4f;
static int g_xe_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

#if DEV && SHOW_AO
static Com_ptr<ID3D11ShaderResourceView> g_srv_ao;
#endif

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

// SMAA
constexpr float g_smaa_clear_color[4] = {};
static SMAA_rt_metrics g_smaa_rt_metrics;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

// Global COM resources.
namespace GCOM
{
	static Com_ptr<ID3D11ComputeShader> cs_linearize;
	static Com_ptr<ID3D11VertexShader> vs_fullscreen_triangle;
	static Com_ptr<ID3D11SamplerState> smp_point;
	static Com_ptr<ID3D11ShaderResourceView> srv_depth;
	static Com_ptr<ID3D11Buffer> cb;

	// Replacement shaders.
	static Com_ptr<ID3D11PixelShader> ps_tonemap_0x29D570D8;
	static Com_ptr<ID3D11PixelShader> ps_starburst_and_lens_dirt_0x2AD953ED;
	static Com_ptr<ID3D11PixelShader> ps_object_highlight_white_0x4D93743F;
	static Com_ptr<ID3D11PixelShader> ps_object_highlight_red_0x61D986B8;

	// XeGTAO
	static Com_ptr<ID3D11ComputeShader> cs_xe_gtao_prefilter_depths16x16;
	static Com_ptr<ID3D11ComputeShader> cs_xe_gtao_main_pass;
	static Com_ptr<ID3D11ComputeShader> cs_xe_gtao_denoise_pass1;
	static Com_ptr<ID3D11ComputeShader> cs_xe_gtao_denoise_pass2;

	// Bloom
	static Com_ptr<ID3D11PixelShader> ps_bloom_downsample;
	static Com_ptr<ID3D11PixelShader> ps_bloom_prefilter;
	static Com_ptr<ID3D11PixelShader> ps_bloom_upsample;
	static Com_ptr<ID3D11Buffer> cb_bloom;
	static Com_ptr<ID3D11BlendState> blend_bloom;

	// SMAA
	static Com_ptr<ID3D11ShaderResourceView> srv_smaa_area_tex;
	static Com_ptr<ID3D11ShaderResourceView> srv_smaa_search_tex;
	static Com_ptr<ID3D11VertexShader> vs_smaa_edge_detection;
	static Com_ptr<ID3D11PixelShader> ps_smaa_edge_detection;
	static Com_ptr<ID3D11VertexShader> vs_smaa_blending_weight_calculation;
	static Com_ptr<ID3D11PixelShader> ps_smaa_blending_weight_calculation;
	static Com_ptr<ID3D11VertexShader> vs_smaa_neighborhood_blending;
	static Com_ptr<ID3D11PixelShader> ps_smaa_neighborhood_blending;
	static Com_ptr<ID3D11DepthStencilState> ds_smaa_disable_depth_replace_stencil;
	static Com_ptr<ID3D11DepthStencilState> ds_smaa_disable_depth_use_stencil;

	static void reset_xe_gtao() noexcept
	{
		cs_xe_gtao_prefilter_depths16x16.reset();
		cs_xe_gtao_main_pass.reset();
		cs_xe_gtao_denoise_pass1.reset();
		cs_xe_gtao_denoise_pass2.reset();
	}

	static void reset_smaa() noexcept
	{
		srv_smaa_area_tex.reset();
		srv_smaa_search_tex.reset();
		vs_smaa_edge_detection.reset();
		ps_smaa_edge_detection.reset();
		vs_smaa_blending_weight_calculation.reset();
		ps_smaa_blending_weight_calculation.reset();
		vs_smaa_neighborhood_blending.reset();
		ps_smaa_neighborhood_blending.reset();
		ds_smaa_disable_depth_replace_stencil.reset();
		ds_smaa_disable_depth_use_stencil.reset();
	}

	static void reset() noexcept
	{
		cs_linearize.reset();
		vs_fullscreen_triangle.reset();
		smp_point.reset();
		srv_depth.reset();
		cb.reset();
		
		// Replacement shaders.
		ps_tonemap_0x29D570D8.reset();
		ps_starburst_and_lens_dirt_0x2AD953ED.reset();
		ps_object_highlight_white_0x4D93743F.reset();
		ps_object_highlight_red_0x61D986B8.reset();

		// Bloom
		ps_bloom_downsample.reset();
		ps_bloom_prefilter.reset();
		ps_bloom_upsample.reset();
		cb_bloom.reset();
		blend_bloom.reset();

		reset_xe_gtao();
		reset_smaa();
	}
};

static void draw_xe_gtao(ID3D11DeviceContext* ctx)
{
	// Original binds:
	// t0 - depth - r32f
	// t1 - normal - rgba8unorm (upgraded to rgba16unorm)
	// u0 - out - rgba8unorm (upgraded to rgba16unorm)

	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());

	// Backup the Out UAV and the Normal SRV since we will overide them,
	// before we will need them. 
	Com_ptr<ID3D11UnorderedAccessView> uav_original;
	ctx->CSGetUnorderedAccessViews(0, 1, uav_original.put());
	Com_ptr<ID3D11ShaderResourceView> srv_normal;
	ctx->CSGetShaderResources(1, 1, srv_normal.put());

	// XeGTAOPrefilterDepths16x16 pass
	//

	// Create sampler.
	[[unlikely]] if (!GCOM::smp_point) {
		create_sampler_point_clamp(device.get(), GCOM::smp_point.put());
	}

	// Create CS.
	[[unlikely]] if (!GCOM::cs_xe_gtao_prefilter_depths16x16) {
		const std::string effect_radius_val = std::to_string(g_xe_gtao_radius);
		const std::array defines = {
			D3D_SHADER_MACRO{ "EFFECT_RADIUS", effect_radius_val.c_str() },
			D3D_SHADER_MACRO{ nullptr, nullptr }
		};
		create_compute_shader(device.get(), GCOM::cs_xe_gtao_prefilter_depths16x16.put(), L"XeGTAO.hlsl", "prefilter_depths16x16_cs", defines.data());
	}

	D3D11_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = g_swapchain_width;
	tex_desc.Height = g_swapchain_height;
	tex_desc.MipLevels = XE_GTAO_DEPTH_MIP_LEVELS;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	// Create prefilter depths views.
	Com_ptr<ID3D11Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
	std::array<ID3D11UnorderedAccessView*, XE_GTAO_DEPTH_MIP_LEVELS> uav_prefilter_depths = {};
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = tex_desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	for (int i = 0; i < uav_prefilter_depths.size(); ++i) {
	   uav_desc.Texture2D.MipSlice = i;
	   ensure(device->CreateUnorderedAccessView(tex.get(), &uav_desc, &uav_prefilter_depths[i]), >= 0);
	}
	Com_ptr<ID3D11ShaderResourceView> srv_prefilter_depths;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_prefilter_depths.put()), >= 0);

	// Bindings.
	ctx->CSSetUnorderedAccessViews(0, uav_prefilter_depths.size(), uav_prefilter_depths.data(), nullptr);
	ctx->CSSetShader(GCOM::cs_xe_gtao_prefilter_depths16x16.get(), nullptr, 0);
	ctx->CSSetSamplers(0, 1, &GCOM::smp_point);

	ctx->Dispatch((tex_desc.Width + 16 - 1) / 16, (tex_desc.Height + 16 - 1) / 16, 1);

	// Unbind UAVs and release uav_prefilter_depths.
	static constexpr std::array<ID3D11UnorderedAccessView*, uav_prefilter_depths.size()> uav_nulls_prefilter_depths_pass = {};
	ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);
	for (int i = 0; i < uav_prefilter_depths.size(); ++i) {
	   uav_prefilter_depths[i]->Release();
	}

	//

	// XeGTAOMainPass pass
	//

	// Create CS.
	[[unlikely]] if (!GCOM::cs_xe_gtao_main_pass) {
		const std::string xe_gtao_quality_val = std::to_string(g_xe_gtao_quality);
		const std::string effect_radius_val = std::to_string(g_xe_gtao_radius);
		const std::array defines = {
			D3D_SHADER_MACRO{ "EFFECT_RADIUS", effect_radius_val.c_str() },
			D3D_SHADER_MACRO{ "XE_GTAO_QUALITY", xe_gtao_quality_val.c_str() },
			D3D_SHADER_MACRO{ nullptr, nullptr }
		};
		create_compute_shader(device.get(), GCOM::cs_xe_gtao_main_pass.put(), L"XeGTAO.hlsl", "main_pass_cs", defines.data());
	}

	// Create AO term and Edges views.
	tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
	tex_desc.MipLevels = 1;
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
	Com_ptr<ID3D11UnorderedAccessView> uav_main_pass;
	ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, uav_main_pass.put()), >= 0);
	Com_ptr<ID3D11ShaderResourceView> srv_main_pass;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_main_pass.put()), >= 0);

	// Bindings.
	ctx->CSSetUnorderedAccessViews(0, 1, &uav_main_pass, nullptr);
	ctx->CSSetShader(GCOM::cs_xe_gtao_main_pass.get(), nullptr, 0);
	const std::array srvs_main_pass = { srv_prefilter_depths.get(), srv_normal.get() };
	ctx->CSSetShaderResources(0, srvs_main_pass.size(), srvs_main_pass.data());

	ctx->Dispatch((tex_desc.Width + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, (tex_desc.Height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);

	//

	// Doing 2 XeGTAODenoisePass passes correspond to "Denoising level: Medium" from the XeGTAO demo.

	// XeGTAODenoisePass1 pass
	//

	// Create CS.
	[[unlikely]] if (!GCOM::cs_xe_gtao_denoise_pass1) {
		static constexpr std::array defines = {
			D3D_SHADER_MACRO{ "XE_GTAO_FINAL_APPLY", "0" },
			D3D_SHADER_MACRO{ nullptr, nullptr }
		};
		create_compute_shader(device.get(), GCOM::cs_xe_gtao_denoise_pass1.put(), L"XeGTAO.hlsl", "denoise_pass_cs", defines.data());
	}

	// Create AO term and Edges views.
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
	Com_ptr<ID3D11UnorderedAccessView> uav_denoise_pass1;
	ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, uav_denoise_pass1.put()), >= 0);
	Com_ptr<ID3D11ShaderResourceView> srv_denoise_pass1;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_denoise_pass1.put()), >= 0);

	// Bindings.
	ctx->CSSetUnorderedAccessViews(0, 1, &uav_denoise_pass1, nullptr);
	ctx->CSSetShader(GCOM::cs_xe_gtao_denoise_pass1.get(), nullptr, 0);
	ctx->CSSetShaderResources(0, 1, &srv_main_pass);

	ctx->Dispatch((tex_desc.Width + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (tex_desc.Height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,1);

	//

	// XeGTAODenoisePass2 pass
	//

	// Create CS.
	[[unlikely]] if (!GCOM::cs_xe_gtao_denoise_pass2) {
		static constexpr std::array defines = {
			D3D_SHADER_MACRO{ "XE_GTAO_FINAL_APPLY", "1" },
			D3D_SHADER_MACRO{ nullptr, nullptr }
		};
		create_compute_shader(device.get(), GCOM::cs_xe_gtao_denoise_pass2.put(), L"XeGTAO.hlsl", "denoise_pass_cs", defines.data());
	}

	// Bindings.
	ctx->CSSetUnorderedAccessViews(0, 1, &uav_original, nullptr);
	ctx->CSSetShader(GCOM::cs_xe_gtao_denoise_pass2.get(), nullptr, 0);
	ctx->CSSetShaderResources(0, 1, &srv_denoise_pass1);

	ctx->Dispatch((tex_desc.Width + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (tex_desc.Height + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);

	//

	#if DEV && SHOW_AO
	Com_ptr<ID3D11Resource> resource;
	uav_original->GetResource(resource.put());
	ensure(device->CreateShaderResourceView(resource.get(), nullptr, g_srv_ao.put()), >= 0);
	#endif
}

static bool on_ps_bloom_prefilter(ID3D11DeviceContext* ctx)
{
	ctx->PSGetConstantBuffers(0, 1, GCOM::cb_bloom.put());
	return false;
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
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
	auto hr = ps->GetPrivateData(g_ps_depth_copy_0x496E549B_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_depth_copy_0x496E549B_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Get depth.
		// RT should be r32f.
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource;
		rtv->GetResource(resource.put());
		ensure(device->CreateShaderResourceView(resource.get(), nullptr, GCOM::srv_depth.put()), >= 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_lens_flare_0xE5427265_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_lens_flare_0xE5427265_hash) {
		if (g_disable_lens_flare) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_starburst_and_lens_dirt_0x2AD953ED_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_starburst_and_lens_dirt_0x2AD953ED_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Create PS.
		[[unlikely]] if (!GCOM::ps_starburst_and_lens_dirt_0x2AD953ED) {
			const std::string disable_lens_dirt = g_disable_lens_dirt ? "1" : "0";
			const std::array defines = {
				D3D_SHADER_MACRO{ "DISABLE_LENS_DIRT", disable_lens_dirt.c_str() },
				D3D_SHADER_MACRO{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), GCOM::ps_starburst_and_lens_dirt_0x2AD953ED.put(), L"Starburst_LensFlare_0x2AD953ED_ps.hlsl", "main", defines.data());
		}

		// Bindings.
		ctx->PSSetShader(GCOM::ps_starburst_and_lens_dirt_0x2AD953ED.get(), nullptr, 0);
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
	auto hr = ps->GetPrivateData(g_ps_bloom_prefilter_dof_0xC39285AF_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_prefilter_dof_0xC39285AF_hash) {
		return on_ps_bloom_prefilter(ctx);
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_prefilter_dof_0x6F4F1E8F_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_prefilter_dof_0x6F4F1E8F_hash) {
		return on_ps_bloom_prefilter(ctx);
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_prefilter_dof_0x9113DE68_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_prefilter_dof_0x9113DE68_hash) {
		return on_ps_bloom_prefilter(ctx);
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_prefilter_dof_0xAD03A911_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_prefilter_dof_0xAD03A911_hash) {
		return on_ps_bloom_prefilter(ctx);
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_object_highlight_white_0x4D93743F_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_object_highlight_white_0x4D93743F_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Create PS.
		[[unlikely]] if (!GCOM::ps_object_highlight_white_0x4D93743F) {
			create_pixel_shader(device.get(), GCOM::ps_object_highlight_white_0x4D93743F.put(), L"ObjectHighlightWhite_0x4D93743F_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(GCOM::ps_object_highlight_white_0x4D93743F.get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_object_highlight_red_0x61D986B8_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_object_highlight_red_0x61D986B8_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Create PS.
		[[unlikely]] if (!GCOM::ps_object_highlight_red_0x61D986B8) {
			create_pixel_shader(device.get(), GCOM::ps_object_highlight_red_0x61D986B8.put(), L"ObjectHighlightRed_0x61D986B8_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(GCOM::ps_object_highlight_red_0x61D986B8.get(), nullptr, 0);

		return false;
	}


	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x29D570D8_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x29D570D8_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("(0x29D570D8)The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(1, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("(0x29D570D8)The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// SRV0 should be the scene in linear color space.
		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		ctx->PSGetShaderResources(0, 1, srv_scene.put());

		// Bloom
		//

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup Viewports.
		UINT num_viewports;
		ctx->RSGetViewports(&num_viewports, nullptr);
		std::vector<D3D11_VIEWPORT> viewports_original(num_viewports);
		ctx->RSGetViewports(&num_viewports, viewports_original.data());

		// Backup Blend.
		Com_ptr<ID3D11BlendState> blend_original;
		FLOAT blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Backup RTs.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		Com_ptr<ID3D11DepthStencilView> dsv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());

		// Create MIPs and views.
		////

		// Get the scene resource and texture description from the SRV.
		Com_ptr<ID3D11Resource> resource;
		srv_scene->GetResource(resource.put());
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		const auto scene_width = tex_desc.Width;
		const auto scene_height = tex_desc.Height;

		// Create Y MIPs and views.
		tex_desc.Width /= 2;
		tex_desc.Height /= 2;
		tex_desc.MipLevels = g_bloom_nmips;
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		const UINT y_mip0_width = tex_desc.Width;
		const UINT y_mip0_height = tex_desc.Height;
		std::vector<ID3D11RenderTargetView*> rtv_mips_y(g_bloom_nmips);
		std::vector<ID3D11ShaderResourceView*> srv_mips_y(g_bloom_nmips);
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = tex_desc.Format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		for (int i = 0; i < g_bloom_nmips; ++i) {
			rtv_desc.Texture2D.MipSlice = i;
			ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &rtv_mips_y[i]), >= 0);
			srv_desc.Texture2D.MostDetailedMip = i;
			ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &srv_mips_y[i]), >= 0);
		}

		// Create X MIPs and views.
		//////

		std::vector<ID3D11RenderTargetView*> rtv_mips_x(g_bloom_nmips);
		std::vector<ID3D11ShaderResourceView*> srv_mips_x(g_bloom_nmips);
		
		// Create X MIP0 and views.
		tex_desc.Width = scene_width / 2;
		tex_desc.Height = scene_height;
		tex_desc.MipLevels = 1;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_mips_x[0]), >= 0);
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_mips_x[0]), >= 0);

		const UINT x_mip0_width = tex_desc.Width;
		const UINT x_mip0_height = tex_desc.Height;

		// Create rest of X MIPs and views.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			tex_desc.Width = std::max(1u, x_mip0_width >> i);
			tex_desc.Height = std::max(1u, x_mip0_height >> i);
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_mips_x[i]), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_mips_x[i]), >= 0);
		}

		//////

		////

		// Create bloom CB.
		[[unlikely]] if (!GCOM::cb) {
			create_constant_buffer(device.get(), sizeof(g_cb_data), GCOM::cb.put());
		}

		// Prefilter + downsample pass
		////

		// Create VS.
		[[unlikely]] if (!GCOM::vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), GCOM::vs_fullscreen_triangle.put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!GCOM::ps_bloom_downsample) {
			create_pixel_shader(device.get(), GCOM::ps_bloom_downsample.put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create PS.
		[[unlikely]] if (!GCOM::ps_bloom_prefilter) {
			create_pixel_shader(device.get(), GCOM::ps_bloom_prefilter.put(), L"Bloom_impl.hlsl", "prefilter_ps");
		}

		D3D11_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(scene_width, scene_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(ctx, GCOM::cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_mips_x[0], nullptr);
		ctx->VSSetShader(GCOM::vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShader(GCOM::ps_bloom_downsample.get(), nullptr, 0);
		const std::array ps_cbs = { GCOM::cb_bloom.get(), GCOM::cb.get() };
		ctx->PSSetConstantBuffers(14 - ps_cbs.size(), ps_cbs.size(), ps_cbs.data());
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
		update_constant_buffer(ctx, GCOM::cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_mips_y[0], nullptr);
		ctx->PSSetShader(GCOM::ps_bloom_prefilter.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_mips_x[0]);
		ctx->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		ctx->Draw(3, 0);

		////

		// Downsample passes
		////

		// Bindings.
		ctx->PSSetShader(GCOM::ps_bloom_downsample.get(), nullptr, 0);

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_data.axis = float2(1.0f, 0.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			g_cb_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(ctx, GCOM::cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &rtv_mips_x[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &srv_mips_y[i - 1]);
			ctx->RSSetViewports(1, &viewport_x);

			// Draw X pass.
			ctx->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, GCOM::cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &rtv_mips_y[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &srv_mips_x[i]);
			ctx->RSSetViewports(1, &viewports_y[i]);

			// Draw Y pass.
			ctx->Draw(3, 0);
		}

		////

		// Upsample passes
		////

		// Create PS.
		[[unlikely]] if (!GCOM::ps_bloom_upsample) {
			create_pixel_shader(device.get(), GCOM::ps_bloom_upsample.put(), L"Bloom_impl.hlsl", "upsample_ps");
		}

		// Create blend.
		if (!GCOM::blend_bloom) {
			CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
			ensure(device->CreateBlendState(&desc, GCOM::blend_bloom.put()), >= 0);
		}

		// Bindings.
		ctx->PSSetShader(GCOM::ps_bloom_upsample.get(), nullptr, 0);

		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// If both dst and src are D3D10_BLEND_BLEND_FACTOR,
			// factor of 0.5 will be enegrgy preserving.
			static constexpr FLOAT blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, GCOM::cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &rtv_mips_y[i - 1], nullptr);
			ctx->PSSetShaderResources(0, 1, &srv_mips_y[i]);
			ctx->RSSetViewports(1, &viewports_y[i - 1]);
			ctx->OMSetBlendState(GCOM::blend_bloom.get(), blend_factor, UINT_MAX);

			ctx->Draw(3, 0);
		}

		// Restore.
		// It's convinient to do a partial restore here.
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);
		ctx->OMSetRenderTargets(1, &rtv_original, dsv_original.get());
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_scene);
		ctx->PSSetShaderResources(3, 1, &srv_mips_y[0]); // Replace the original bloom.
		ctx->RSSetViewports(viewports_original.size(), viewports_original.data());

		// Release com arrays.
		auto release_com_array = [](auto& array){ for (auto* p : array) if (p) p->Release(); };
		release_com_array(rtv_mips_x);
		release_com_array(srv_mips_x);
		release_com_array(rtv_mips_y);
		release_com_array(srv_mips_y);

		//

		// Tonemap_0x29D570D8 pass
		//

		// Create PS.
		[[unlikely]] if (!GCOM::ps_tonemap_0x29D570D8) {
			const std::string bloom_intensity_val = std::to_string(g_bloom_intensity);
			const std::array defines = {
				D3D_SHADER_MACRO{ "BLOOM_INTENSITY", bloom_intensity_val.c_str() },
				D3D_SHADER_MACRO{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), GCOM::ps_tonemap_0x29D570D8.put(), L"Tonemap_0x29D570D8_ps.hlsl", "main", defines.data());
		}

		// Bindings.
		ctx->PSSetShader(GCOM::ps_tonemap_0x29D570D8.get(), nullptr, 0);

		//

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fxaa_0x27BD2A2E_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fxaa_0x27BD2A2E_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("(0x27BD2A2E) The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(0, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("(0x27BD2A2E)The expected sampler in the slot 0 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Backup RT.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// SRV0 should be tonemapped scene.
		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		ctx->PSGetShaderResources(0, 1, srv_scene.put());

		#if DEV && SHOW_AO
		srv_scene = g_srv_ao;
		#endif

		// Get the scene resource and texture description from SRV.
		Com_ptr<ID3D11Resource> resource;
		srv_scene->GetResource(resource.put());
		Com_ptr<ID3D11Texture2D> tex;
		resource->QueryInterface(tex.put());
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Linearize pass
		//

		// Create CS.
		[[unlikely]] if (!GCOM::cs_linearize) {
			create_compute_shader(device.get(), GCOM::cs_linearize.put(), L"Linearize_cs.hlsl");
		}

		// Create UAs and views.
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11UnorderedAccessView> uav_scene_linearized;
		ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, uav_scene_linearized.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_scene_linearized;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_scene_linearized.put()), >= 0);
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

		// Bindings.
		static constexpr ID3D11ShaderResourceView* srv_null = nullptr;
		ctx->PSSetShaderResources(0, 1, &srv_null);
		ctx->CSSetShader(GCOM::cs_linearize.get(), nullptr, 0);
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_scene_linearized, nullptr);
		ctx->CSSetShaderResources(0, 1, &srv_scene);

		ctx->Dispatch((tex_desc.Width + 8 - 1) / 8, (tex_desc.Height + 8 - 1) / 8, 1);

		// Unbind UAVs and SRVs.
		constexpr ID3D11UnorderedAccessView* uav_null = nullptr;
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);
		ctx->CSSetShaderResources(0, 1, &srv_null);

		//

		// SMAAEdgeDetection pass
		//

		// Create point sampler.
		[[unlikely]] if (!GCOM::smp_point) {
			create_sampler_point_clamp(device.get(), GCOM::smp_point.put());
		}

		// Create VS.
		[[unlikely]] if (!GCOM::vs_smaa_edge_detection) {
			g_smaa_rt_metrics.set(tex_desc.Width, tex_desc.Height);
			create_vertex_shader(device.get(), GCOM::vs_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!GCOM::ps_smaa_edge_detection) {
			create_pixel_shader(device.get(), GCOM::ps_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!GCOM::ds_smaa_disable_depth_replace_stencil) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, GCOM::ds_smaa_disable_depth_replace_stencil.put()), >= 0);
		}

		// Create DSV.
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11DepthStencilView> dsv;
		ensure(device->CreateDepthStencilView(tex.get(), nullptr, dsv.put()), >= 0);

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_edge_detection;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_edge_detection.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_edge_detection;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_edge_detection.put()), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(GCOM::ds_smaa_disable_depth_replace_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_edge_detection, dsv.get());
		ctx->VSSetShader(GCOM::vs_smaa_edge_detection.get(), nullptr, 0);
		ctx->PSSetShader(GCOM::ps_smaa_edge_detection.get(), nullptr, 0);
		ctx->PSSetSamplers(1, 1, &GCOM::smp_point);
		const std::array ps_srvs_smaa_edge_detection = { srv_scene.get(), GCOM::srv_depth.get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_edge_detection.size(), ps_srvs_smaa_edge_detection.data());

		ctx->ClearRenderTargetView(rtv_smaa_edge_detection.get(), g_smaa_clear_color);
		ctx->ClearDepthStencilView(dsv.get(), D3D11_CLEAR_STENCIL, 1.0f, 0);
		ctx->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!GCOM::vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device.get(), GCOM::vs_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!GCOM::ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device.get(), GCOM::ps_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!GCOM::srv_smaa_area_tex) {
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
			ensure(device->CreateTexture2D(&desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, GCOM::srv_smaa_area_tex.put()), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!GCOM::srv_smaa_search_tex) {
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
			ensure(device->CreateTexture2D(&desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, GCOM::srv_smaa_search_tex.put()), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!GCOM::ds_smaa_disable_depth_use_stencil) {
			CD3D11_DEPTH_STENCIL_DESC desc(D3D11_DEFAULT);
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, GCOM::ds_smaa_disable_depth_use_stencil.put()), >= 0);
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_blending_weight_calculation;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_blending_weight_calculation.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_blending_weight_calculation;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_blending_weight_calculation.put()), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(GCOM::ds_smaa_disable_depth_use_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_blending_weight_calculation, dsv.get());
		ctx->VSSetShader(GCOM::vs_smaa_blending_weight_calculation.get(), nullptr, 0);
		ctx->PSSetShader(GCOM::ps_smaa_blending_weight_calculation.get(), nullptr, 0);
		const std::array ps_srvs_smaa_blending_weight_calculation = { srv_smaa_edge_detection.get(), GCOM::srv_smaa_area_tex.get(), GCOM::srv_smaa_search_tex.get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_blending_weight_calculation.size(), ps_srvs_smaa_blending_weight_calculation.data());

		ctx->ClearRenderTargetView(rtv_smaa_blending_weight_calculation.get(), g_smaa_clear_color);
		ctx->Draw(3, 0);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create VS.
		[[unlikely]] if (!GCOM::vs_smaa_neighborhood_blending) {
			create_vertex_shader(device.get(), GCOM::vs_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!GCOM::ps_smaa_neighborhood_blending) {
			create_pixel_shader(device.get(), GCOM::ps_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->VSSetShader(GCOM::vs_smaa_neighborhood_blending.get(), nullptr, 0);
		ctx->PSSetShader(GCOM::ps_smaa_neighborhood_blending.get(), nullptr, 0);
		const std::array ps_srvs_neighborhood_blending = { srv_scene_linearized.get(), srv_smaa_blending_weight_calculation.get() };
		ctx->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

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
	auto hr = cs->GetPrivateData(g_cs_ao_main_high_0x1E7B9941_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ao_main_high_0x1E7B9941_hash) {
		draw_xe_gtao(ctx);
		return true;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ao_main_ultra_0x348372D0_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ao_main_ultra_0x348372D0_hash) {
		draw_xe_gtao(ctx);
		return true;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ao_denoise1_0xF6ED18D8_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ao_denoise1_0xF6ED18D8_hash) {
		return true;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ao_denoise2_0xBA9A4DB1_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ao_denoise2_0xBA9A4DB1_hash) {
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
				case g_ps_depth_copy_0x496E549B_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_depth_copy_0x496E549B_guid, sizeof(g_ps_depth_copy_0x496E549B_hash), &g_ps_depth_copy_0x496E549B_hash), >= 0);
					break;
				case g_ps_tonemap_0x29D570D8_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x29D570D8_guid, sizeof(g_ps_tonemap_0x29D570D8_hash), &g_ps_tonemap_0x29D570D8_hash), >= 0);
					break;
				case g_ps_fxaa_0x27BD2A2E_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fxaa_0x27BD2A2E_guid, sizeof(g_ps_fxaa_0x27BD2A2E_hash), &g_ps_fxaa_0x27BD2A2E_hash), >= 0);
					break;
				case g_ps_lens_flare_0xE5427265_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_lens_flare_0xE5427265_guid, sizeof(g_ps_lens_flare_0xE5427265_hash), &g_ps_lens_flare_0xE5427265_hash), >= 0);
					break;
				case g_ps_bloom_prefilter_dof_0xC39285AF_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_prefilter_dof_0xC39285AF_guid, sizeof(g_ps_bloom_prefilter_dof_0xC39285AF_hash), &g_ps_bloom_prefilter_dof_0xC39285AF_hash), >= 0);
					break;
				case g_ps_bloom_prefilter_dof_0x6F4F1E8F_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_prefilter_dof_0x6F4F1E8F_guid, sizeof(g_ps_bloom_prefilter_dof_0x6F4F1E8F_hash), &g_ps_bloom_prefilter_dof_0x6F4F1E8F_hash), >= 0);
					break;
				case g_ps_bloom_prefilter_dof_0x9113DE68_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_prefilter_dof_0x9113DE68_guid, sizeof(g_ps_bloom_prefilter_dof_0x9113DE68_hash), &g_ps_bloom_prefilter_dof_0x9113DE68_hash), >= 0);
					break;
				case g_ps_bloom_prefilter_dof_0xAD03A911_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_prefilter_dof_0xAD03A911_guid, sizeof(g_ps_bloom_prefilter_dof_0xAD03A911_hash), &g_ps_bloom_prefilter_dof_0xAD03A911_hash), >= 0);
					break;
				case g_ps_starburst_and_lens_dirt_0x2AD953ED_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_starburst_and_lens_dirt_0x2AD953ED_guid, sizeof(g_ps_starburst_and_lens_dirt_0x2AD953ED_hash), &g_ps_starburst_and_lens_dirt_0x2AD953ED_hash), >= 0);
					break;
				case g_ps_object_highlight_white_0x4D93743F_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_object_highlight_white_0x4D93743F_guid, sizeof(g_ps_object_highlight_white_0x4D93743F_hash), &g_ps_object_highlight_white_0x4D93743F_hash), >= 0);
					break;
				case g_ps_object_highlight_red_0x61D986B8_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_object_highlight_red_0x61D986B8_guid, sizeof(g_ps_object_highlight_red_0x61D986B8_hash), &g_ps_object_highlight_red_0x61D986B8_hash), >= 0);
					break;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_ao_main_high_0x1E7B9941_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ao_main_high_0x1E7B9941_guid, sizeof(g_cs_ao_main_high_0x1E7B9941_hash), &g_cs_ao_main_high_0x1E7B9941_hash), >= 0);
					break;
				case g_cs_ao_main_ultra_0x348372D0_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ao_main_ultra_0x348372D0_guid, sizeof(g_cs_ao_main_ultra_0x348372D0_hash), &g_cs_ao_main_ultra_0x348372D0_hash), >= 0);
					break;
				case g_cs_ao_denoise1_0xF6ED18D8_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ao_denoise1_0xF6ED18D8_guid, sizeof(g_cs_ao_denoise1_0xF6ED18D8_hash), &g_cs_ao_denoise1_0xF6ED18D8_hash), >= 0);
					break;
				case g_cs_ao_denoise2_0xBA9A4DB1_hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ao_denoise2_0xBA9A4DB1_guid, sizeof(g_cs_ao_denoise2_0xBA9A4DB1_hash), &g_cs_ao_denoise2_0xBA9A4DB1_hash), >= 0);
					break;
			}
		}
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	bool is_size = resource_desc.texture.width == 128 && resource_desc.texture.height == 96;
	if (((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) && !is_size) {
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
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs.
	// The game uses r8g8b8a8_unorm_srgb views so don't upgrade r8g8b8a8_typeless.
	// Upgradeing textures of size 128x96 crashes the game.
	bool is_size = desc.texture.width == 128 && desc.texture.height == 96;
	if (((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) && !is_size) {
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
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
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_refresh_rate = 0.0f;
	desc.sync_interval = 0;

	// Save swapchain size.
	g_swapchain_width = desc.back_buffer.texture.width;
	g_swapchain_height = desc.back_buffer.texture.height;

	// Reset reolution dependent resources.
	GCOM::reset_xe_gtao();
	GCOM::reset_smaa();

	return true;
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
	GCOM::reset();
}

static void read_config()
{	
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
	}
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensDirt", g_disable_lens_dirt);
	}
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	#if DEV
	if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1, 10)) {
		g_bloom_sigmas.resize(g_bloom_nmips);
	}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	}
	ImGui::Spacing();
	#endif

	if (ImGui::SliderFloat("XeGTAO radius", &g_xe_gtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAORadius", g_xe_gtao_radius);
		GCOM::reset_xe_gtao();
	}
	static constexpr std::array xe_gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO quality", &g_xe_gtao_quality, xe_gtao_quality_items.data(), xe_gtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "XeGTAOQuality", g_xe_gtao_quality);
		GCOM::reset_xe_gtao();
	}
	ImGui::Spacing();
	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		GCOM::ps_tonemap_0x29D570D8.reset();
	}
	ImGui::Spacing();
	if (ImGui::Checkbox("Disable Lens Flare", &g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
	}
	if (ImGui::Checkbox("Disable Lens Dirt", &g_disable_lens_dirt)) {
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "DisableLensDirt", g_disable_lens_flare);
		GCOM::ps_starburst_and_lens_dirt_0x2AD953ED.reset();
	}
	ImGui::Spacing();
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 10.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "BioShockInfiniteGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "BioShockInfiniteGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "BioShockInfiniteGraphicalUpgrade v1.1.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BioShockInfiniteGraphicalUpgrade";

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
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 1.5f;
			g_bloom_sigmas[1] = 1.0f;
			g_bloom_sigmas[2] = 1.0f;
			g_bloom_sigmas[3] = 1.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			// Events.
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
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
