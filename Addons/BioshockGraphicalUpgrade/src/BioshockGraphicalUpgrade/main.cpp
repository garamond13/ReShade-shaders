#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "SMAA/SMAA.h"

extern "C" __declspec(dllexport) const char* NAME = "BioshockGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v9.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BioshockGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_fog_0xB69D6558 = { 0xB69D6558, { 0x6920fbb, 0xcfa4, 0x48b8, { 0x94, 0xf9, 0x80, 0x7, 0x83, 0xe2, 0xbc, 0xfb }}};
constexpr Shader_hash g_ps_fog_0xE2683E33 = { 0xE2683E33, { 0x7bc430f7, 0x5db5, 0x4b25, { 0x83, 0x90, 0xc8, 0x9c, 0x74, 0xc5, 0xa7, 0xf }}};
constexpr Shader_hash g_ps_fog_0xC907CF9A = { 0xC907CF9A, { 0x4875f184, 0xadee, 0x4f40, { 0xb6, 0xf5, 0xfa, 0xf8, 0x54, 0xcd, 0xb4, 0x15 }}};
constexpr Shader_hash g_ps_fog_0x9CFB96DA = { 0x9CFB96DA, { 0xfd87086, 0xb29a, 0x4c23, { 0xa9, 0x30, 0x8e, 0x9a, 0x98, 0x59, 0x15, 0x88 }}};
constexpr Shader_hash g_ps_fog_0x3B177042 = { 0x3B177042, { 0x3897d2ee, 0xb12, 0x4dfc, { 0xa8, 0xa0, 0xae, 0x50, 0x1e, 0x17, 0x3c, 0x3 }}};
constexpr Shader_hash g_ps_fog_0xEB7BE1D6 = { 0xEB7BE1D6, { 0x9eaab4f0, 0x8224, 0x4a26, { 0xa4, 0x11, 0x2a, 0x98, 0x9f, 0x7d, 0xb8, 0xaf }}};
constexpr Shader_hash g_ps_fog_0xF5E21918 = { 0xF5E21918, { 0x973eae08, 0x71f3, 0x492a, { 0x94, 0xe7, 0x94, 0xb, 0x15, 0xe8, 0x7b, 0xbc }}};

constexpr Shader_hash g_ps_ceiling_debris_0xDC232D31 = { 0xDC232D31, { 0xa7cc6654, 0xa644, 0x4908, { 0xb1, 0xf4, 0x6a, 0x9e, 0x59, 0x84, 0xf8, 0xb5 }}};
constexpr Shader_hash g_ps_r_glass_door_0x59018C97 = { 0x59018C97, { 0x83c202f5, 0x2883, 0x4f24, { 0x83, 0xdb, 0x1c, 0x6e, 0xb3, 0x72, 0x6, 0x7 }}};
constexpr Shader_hash g_ps_godrays_0xE689FDF8 = { 0xE689FDF8, { 0xeda804e9, 0xb9ab, 0x487e, { 0xa9, 0x45, 0x6a, 0xf2, 0x6e, 0xd5, 0x81, 0xef }}};
constexpr Shader_hash g_ps_flashing_images_0x7862AA89 = { 0x7862AA89, { 0xb012ee87, 0x13a1, 0x4c5f, { 0x8a, 0xc4, 0x5b, 0x14, 0x70, 0xc4, 0x66, 0x65 }}};

constexpr Shader_hash g_ps_bloom_downsample_0xB51C436B = { 0xB51C436B, { 0xf6b85c15, 0x7efe, 0x4504, { 0x9a, 0xa5, 0xd7, 0xa9, 0x31, 0x7, 0xc, 0xb3 }}};
constexpr Shader_hash g_ps_bloom_0xB1DCCAE7 = { 0xB1DCCAE7, { 0x83883084, 0x9396, 0x4368, { 0xac, 0x72, 0x12, 0x74, 0xe9, 0x65, 0x25, 0x2a }}};
constexpr Shader_hash g_ps_bloom_0x1A782DB1 = { 0x1A782DB1, { 0x7f42b436, 0x268d, 0x4967, { 0xa5, 0xf, 0x0, 0x4b, 0x9f, 0x47, 0x5d, 0x45 }}};
constexpr Shader_hash g_ps_bloom_0xBD24CC87 = { 0xBD24CC87, { 0x96e8e31a, 0x856c, 0x4044, { 0xa6, 0xc2, 0xda, 0x22, 0xfe, 0x9b, 0xe2, 0xd5 }}};

constexpr Shader_hash g_ps_tonemap_0x87A0B43D = { 0x87A0B43D, { 0x476ef3b9, 0xe14e, 0x49f2, { 0xa6, 0x5c, 0x8c, 0x7a, 0x41, 0xd4, 0xa6, 0x12 }}};

//

static ID3D10Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D10Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;

// GTAO
constexpr int GTAO_DEPTH_MIP_LEVELS = 5;
static bool g_gtao_enable = true;
static float g_gtao_fov_y = 47.0f; // in degrees
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
static bool g_has_drawn_gtao;

// Bloom
static int g_bloom_nmips = 6;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

// SMAA
static SMAA_rt_metrics g_smaa_rt_metrics;

// AMD FFX CAS
static float g_amd_ffx_cas_sharpness = 0.0f;

// AMD FFX LFGA
static float g_amd_ffx_lfga_amount = 0.3f;

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

// Device resources.
static std::array<ID3D10RenderTargetView*, GTAO_DEPTH_MIP_LEVELS> g_rtv_gtao_working_depth_mips;
static std::array<ID3D10ShaderResourceView*, GTAO_DEPTH_MIP_LEVELS> g_srv_gtao_working_depth_mips;
static std::vector<ID3D10RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D10ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D10RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D10ShaderResourceView*> g_srv_bloom_mips_x;

static void draw_gtao(ID3D10Device* device, ID3D10RenderTargetView*const* rtv)
{
	// Backup states.
	//

	// Primitive topology.
	D3D10_PRIMITIVE_TOPOLOGY primitive_topology_original;
	device->IAGetPrimitiveTopology(&primitive_topology_original);

	// VS.
	Com_ptr<ID3D10VertexShader> vs_original;
	device->VSGetShader(vs_original.put());

	// Viewports.
	UINT nviewports;
	device->RSGetViewports(&nviewports, nullptr);
	std::vector<D3D10_VIEWPORT> viewports_original(nviewports);
	device->RSGetViewports(&nviewports, viewports_original.data());

	// Rasterizer.
	Com_ptr<ID3D10RasterizerState> rasterizer_original;
	device->RSGetState(rasterizer_original.put());

	// PS.
	Com_ptr<ID3D10PixelShader> ps_original;
	device->PSGetShader(ps_original.put());
	std::array<ID3D10ShaderResourceView*, 2> srvs_original = {};
	device->PSGetShaderResources(0, srvs_original.size(), srvs_original.data());
	Com_ptr<ID3D10SamplerState> smp_original;
	device->PSGetSamplers(0, 1, smp_original.put());

	// Blend.
	Com_ptr<ID3D10BlendState> blend_original;
	FLOAT blend_factor_original[4];
	UINT sample_mask_original;
	device->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

	// RTs.
	Com_ptr<ID3D10RenderTargetView> rtv_original;
	Com_ptr<ID3D10DepthStencilView> dsv_original;
	device->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());
	device->OMSetRenderTargets(0, nullptr, nullptr);

	//

	// PrefilterDepths passes.
	//

	// Create and bind Fullscreen Triangle VS.
	[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
		create_vertex_shader(device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
	}

	// Create prefilter depths PS, for mip0.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h]) {
		create_pixel_shader(device, g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths_mip0_ps");
	}

	// Create prefilter depths PS, for mips 1 to 4.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h]) {
		create_pixel_shader(device, g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths_ps");
	}

	// Create point clamp sampler.
	[[unlikely]] if (!g_managed_resources.samplers["point_clamp"_h]) {
		create_sampler_point_clamp(device, g_managed_resources.samplers["point_clamp"_h].put());
	}

	[[unlikely]] if (!g_rtv_gtao_working_depth_mips[0]) {
		// Create texture.
		D3D10_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = g_swapchain_width;
		tex_desc.Height = g_swapchain_height;
		tex_desc.ArraySize = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		tex_desc.MipLevels = GTAO_DEPTH_MIP_LEVELS;
		tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
		Com_ptr<ID3D10Texture2D> tex;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

		// Create RTVs and SRVs for working depth mips.
		D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = tex_desc.Format;
		rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		for (UINT i = 0; i < GTAO_DEPTH_MIP_LEVELS; ++i) {
			rtv_desc.Texture2D.MipSlice = i;
			ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_gtao_working_depth_mips[i]), >= 0);
			srv_desc.Texture2D.MostDetailedMip = i;
			ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_gtao_working_depth_mips[i]), >= 0);
		}

		// Create working depth SRV with all mips.
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_working_depth"_h].put()), >= 0);
	}

	// Prefilter depths viewport, for mip0.
	D3D10_VIEWPORT viewport = {};
	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Bindings.
	device->OMSetRenderTargets(1, &g_rtv_gtao_working_depth_mips[0], nullptr);
	device->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
	device->PSSetShader(g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h].get());
	device->PSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
	device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["depth"_h]);
	device->RSSetState(nullptr);
	device->RSSetViewports(1, &viewport);
	device->OMSetBlendState(nullptr, nullptr, UINT_MAX);

	// Prefilter depths, mip0.
	device->Draw(3, 0);

	// Bindings.
	device->PSSetShader(g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h].get());

	// Prefilter depths, mips 1 to GTAO_DEPTH_MIP_LEVELS.
	for (int i = 1; i < GTAO_DEPTH_MIP_LEVELS; ++i) {
		viewport.Width = std::max(1, g_swapchain_width >> i);
		viewport.Height = std::max(1, g_swapchain_height >> i);

		// Bindings.
		device->OMSetRenderTargets(1, &g_rtv_gtao_working_depth_mips[i], nullptr);
		device->RSSetViewports(1, &viewport);
		device->PSSetShaderResources(0, 1, &g_srv_gtao_working_depth_mips[i - 1]);

		device->Draw(3, 0);
	}

	//

	// MainPass pass.
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_main_pass"_h]) {
		const float tan_half_fov_y = std::tan(g_gtao_fov_y * std::numbers::pi_v<float> / 180.0f * 0.5f); // radians = degrees * pi / 180.
		const float tan_half_fov_x = tan_half_fov_y * (float)g_swapchain_width / (float)g_swapchain_height;
		const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
		const std::string ndc_to_view_mul_str = std::format("float2({},{})", tan_half_fov_x * 2.0f, tan_half_fov_y * -2.0f);
		const std::string ndc_to_view_add_str = std::format("float2({},{})", -tan_half_fov_x, tan_half_fov_y);
		const std::string quality_str = std::to_string(g_gtao_quality);
		D3D_SHADER_MACRO defines[] = {
			{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
			{ "NDC_TO_VIEW_MUL", ndc_to_view_mul_str.c_str() },
			{ "NDC_TO_VIEW_ADD", ndc_to_view_add_str.c_str() },
			{ "GTAO_QUALITY", quality_str.c_str() },
			{ nullptr, nullptr }
		};
		create_pixel_shader(device, g_managed_resources.pixel_shaders["gtao_main_pass"_h].put(), L"GTAO_impl.hlsl", "main_pass_ps", defines);
	}

	// Create RT views.
	[[unlikely]] if (!g_managed_resources.render_target_views["gtao_main_pass"_h]) {
		D3D10_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = g_swapchain_width;
		tex_desc.Height = g_swapchain_height;
		tex_desc.ArraySize = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		tex_desc.MipLevels = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		Com_ptr<ID3D10Texture2D> tex;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["gtao_main_pass"_h].put()), >= 0);
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_main_pass"_h].put()), >= 0);
	}

	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Bindings.
	device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["gtao_main_pass"_h], nullptr);
	device->PSSetShader(g_managed_resources.pixel_shaders["gtao_main_pass"_h].get());
	device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_working_depth"_h]);
	device->RSSetViewports(1, &viewport);

	device->Draw(3, 0);

	//

	// Doing 2 XeGTAO denoise passes correspond to "Denoising level: Medium" from the XeGTAO demo.

	// DenoisePass1 pass.
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h]) {
		create_pixel_shader(device, g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_ps");
	}

	// Create RT views.
	[[unlikely]] if (!g_managed_resources.render_target_views["gtao_denoise1_pass"_h]) {
		D3D10_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = g_swapchain_width;
		tex_desc.Height = g_swapchain_height;
		tex_desc.ArraySize = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		tex_desc.MipLevels = 1;
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		Com_ptr<ID3D10Texture2D> tex;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["gtao_denoise1_pass"_h].put()), >= 0);
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_denoise1_pass"_h].put()), >= 0);
	}

	// Bindings.
	device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["gtao_denoise1_pass"_h], nullptr);
	device->PSSetShader(g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h].get());
	device->PSSetShaderResources(1, 1, &g_managed_resources.shader_resource_views["gtao_main_pass"_h]);

	device->Draw(3, 0);

	//

	// DenoisePass2 pass
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h]) {
		const D3D_SHADER_MACRO defines[] = {
			{ "FINAL_APPLY", "" },
			{ nullptr, nullptr }
		};
		create_pixel_shader(device, g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_ps", defines);
	}

	// Create blend.
	[[unlikely]] if (!g_managed_resources.blends["gtao"_h]) {
		auto blend_desc = default_D3D10_BLEND_DESC();
		blend_desc.BlendEnable[0] = TRUE;
		blend_desc.SrcBlend = D3D10_BLEND_DEST_COLOR;
		ensure(device->CreateBlendState(&blend_desc, g_managed_resources.blends["gtao"_h].put()), >= 0);
	}

	// Bindings.
	////

	device->OMSetRenderTargets(1, rtv, nullptr);
	device->PSSetShader(g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h].get());
	device->PSSetShaderResources(1, 1, &g_managed_resources.shader_resource_views["gtao_denoise1_pass"_h]);

	#if !(DEV && SHOW_AO)
	device->OMSetBlendState(g_managed_resources.blends["gtao"_h].get(), nullptr, UINT_MAX);
	#endif

	////

	device->Draw(3, 0);

	//

	// Restore states.
	device->OMSetRenderTargets(1, &rtv_original, dsv_original.get());
	device->IASetPrimitiveTopology(primitive_topology_original);
	device->VSSetShader(vs_original.get());
	device->RSSetViewports(nviewports, viewports_original.data());
	device->RSSetState(rasterizer_original.get());
	device->PSSetShader(ps_original.get());
	device->PSSetShaderResources(0, srvs_original.size(), srvs_original.data());
	device->PSSetSamplers(0, 1, &smp_original);
	device->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

	release_com_array(srvs_original);
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	g_has_drawn_gtao = false;

	// We have to rebind RTV and DSV after present for `DXGI_SWAP_EFFECT_FLIP_DISCARD` to work properly in this game.
	g_device->OMGetRenderTargets(1, g_managed_resources.render_target_views["on_present"_h].put(), g_managed_resources.depth_stencil_views["on_present"_h].put());

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

static void on_finish_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain)
{
	g_device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["on_present"_h], g_managed_resources.depth_stencil_views["on_present"_h].get());
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(ps.put());
	if (!ps) {
		return false;
	}

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xE2683E33.guid, &size, &hash);
	if (g_gtao_enable && !g_has_drawn_gtao && SUCCEEDED(hr) && hash == g_ps_fog_0xE2683E33.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_downsample_0xB51C436B.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_downsample_0xB51C436B.hash) {
		// Draw GTAO.
		if (g_gtao_enable && !g_has_drawn_gtao) {
			// We expect SRV to be the main scene.
			Com_ptr<ID3D10ShaderResourceView> srv_original;
			device->PSGetShaderResources(0, 1, srv_original.put());

			// Get the resource and create RTV.
			Com_ptr<ID3D10Resource> resource;
			srv_original->GetResource(resource.put());
			Com_ptr<ID3D10RenderTargetView> rtv;
			ensure(device->CreateRenderTargetView(resource.get(), nullptr, rtv.put()), >= 0);

			draw_gtao(device, &rtv);
			g_has_drawn_gtao = true;
		}

		device->PSGetConstantBuffers(0, 1, g_managed_resources.buffers["cb0_bloom_downsample_0xB51C436B"_h].put());
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xB1DCCAE7.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xB1DCCAE7.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x1A782DB1.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x1A782DB1.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xBD24CC87.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xBD24CC87.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x87A0B43D.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x87A0B43D.hash) {
		// We expect RTV to be the back buffer.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		#if DEV && SHOW_AO
		draw_gtao(device, &rtv_original);
		return true;
		#endif

		Com_ptr<ID3D10VertexShader> vs_original;
		device->VSGetShader(vs_original.put());

		// We expect SRV0 to be the scene.
		Com_ptr<ID3D10ShaderResourceView> srv_scene;
		device->PSGetShaderResources(0, 1, srv_scene.put());

		#if DEV
		// Primitive topology should be D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology;
		device->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(0, 1, smp.put());
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		device->PSGetSamplers(1, 1, smp.put());
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Bloom
		//

		const UINT y_mip0_width = g_swapchain_width >> 1;
		const UINT y_mip0_height = g_swapchain_height >> 1;

		// Create Y MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_y[0]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = y_mip0_width;
			tex_desc.Height = y_mip0_height;
			tex_desc.MipLevels = g_bloom_nmips;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = tex_desc.Format;
			rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
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
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = x_mip0_width;
			tex_desc.Height = x_mip0_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[0]), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[0]), >= 0);
			for (UINT i = 1; i < g_bloom_nmips; ++i) {
				tex_desc.Width = std::max(1u, x_mip0_width >> i);
				tex_desc.Height = std::max(1u, x_mip0_height >> i);
				ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[i]), >= 0);
				ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[i]), >= 0);
			}
		}

		// Prefilter + downsample pass
		////

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create downsample PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_downsample"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["bloom_downsample"_h].put(), L"Bloom_ps.hlsl", "bloom_downsample_ps");
		}

		// Create prefilter PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_prefilter"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["bloom_prefilter"_h].put(), L"Bloom_ps.hlsl", "bloom_prefilter_ps");
		}

		// Create constant buffer.
		[[unlikely]] if (!g_cb) {
			create_constant_buffer(device, sizeof(g_cb_data), g_cb.put());
		}

		D3D10_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(g_swapchain_width, g_swapchain_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		device->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[0], nullptr);
		device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get());
		const std::array bloom_cbs = { g_managed_resources.buffers["cb0_bloom_downsample_0xB51C436B"_h].get(), g_cb.get() };
		device->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT - bloom_cbs.size() + 1, bloom_cbs.size(), bloom_cbs.data());
		device->PSSetShaderResources(0, 1, &srv_scene);
		device->RSSetViewports(1, &viewport_x);

		// Draw X pass.
		device->Draw(3, 0);

		std::vector<D3D10_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(x_mip0_width, x_mip0_height);
		g_cb_data.axis = float2(0.0f, 1.0f);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		device->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[0], nullptr);
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_prefilter"_h].get());
		device->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);
		device->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		device->Draw(3, 0);

		////

		// Downsample passes
		////

		// Common bindings.
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get());

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_data.axis = float2(1.0f, 0.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			g_cb_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
		    device->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[i], nullptr);
		    device->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i - 1]);
		    device->RSSetViewports(1, &viewport_x);

			// Draw X pass.
		    device->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
		    device->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i], nullptr);
		    device->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);
		    device->RSSetViewports(1, &viewports_y[i]);

			// Draw Y pass.
		    device->Draw(3, 0);
		}

		////

		// Upsample passes
		////

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_upsample"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["bloom_upsample"_h].put(), L"Bloom_ps.hlsl", "bloom_upsample_ps");
		}

		// Create blend.
		[[unlikely]] if (!g_managed_resources.blends["bloom"_h]) {
			auto blend_desc = default_D3D10_BLEND_DESC();
			blend_desc.BlendEnable[0] = TRUE;
			blend_desc.SrcBlend = D3D10_BLEND_BLEND_FACTOR;
			blend_desc.DestBlend = D3D10_BLEND_BLEND_FACTOR;
			ensure(device->CreateBlendState(&blend_desc, g_managed_resources.blends["bloom"_h].put()), >= 0);
		}

		// If both dst and src are D3D10_BLEND_BLEND_FACTOR
		// factor of 0.5 is enegrgy preserving.
		static constexpr FLOAT bloom_blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

		// Common bindings.
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_upsample"_h].get());
		device->OMSetBlendState(g_managed_resources.blends["bloom"_h].get(), bloom_blend_factor, UINT_MAX);

		// Render upsample passes.
		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			device->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i - 1], nullptr);
		    device->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i]);
		    device->RSSetViewports(1, &viewports_y[i - 1]);

		    device->Draw(3, 0);
		}

		////

		// Reset viewport.
		D3D10_VIEWPORT viewport = {};
		viewport.Width = g_swapchain_width;
		viewport.Height = g_swapchain_height;
		device->RSSetViewports(1, &viewport);

		// Reset blend state.
		device->OMSetBlendState(nullptr, nullptr, UINT_MAX);

		//

		// SMAAPrePass pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_pre_pass"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["smaa_pre_pass"_h].put(), L"SMAA_impl.hlsl", "smaa_pre_pass_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["smaa_srgb_scene"_h]) {
			// Create texture.
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["smaa_srgb_scene"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_srgb_scene"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["smaa_srgb_scene"_h], nullptr);
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_pre_pass"_h].get());
		device->PSSetShaderResources(0, 1, &srv_scene);

		device->Draw(3, 0);

		//

		// SMAAEdgeDetection pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["smaa_edge_detection"_h]) {
			create_vertex_shader(device, g_managed_resources.vertex_shaders["smaa_edge_detection"_h].put(), L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_edge_detection"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["smaa_edge_detection"_h].put(), L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!g_managed_resources.depth_stencils["smaa_disable_depth_replace_stencil"_h]) {
			D3D10_DEPTH_STENCIL_DESC desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, g_managed_resources.depth_stencils["smaa_disable_depth_replace_stencil"_h].put()), >= 0);
		}

		// Create DSV.
		[[unlikely]] if (!g_managed_resources.depth_stencil_views["smaa"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateDepthStencilView(tex.get(), nullptr, g_managed_resources.depth_stencil_views["smaa"_h].put()), >= 0);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["smaa_edge_detection"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["smaa_edge_detection"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_edge_detection"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["smaa_edge_detection"_h], g_managed_resources.depth_stencil_views["smaa"_h].get());
		device->OMSetDepthStencilState(g_managed_resources.depth_stencils["smaa_disable_depth_replace_stencil"_h].get(), 1);
		device->VSSetShader(g_managed_resources.vertex_shaders["smaa_edge_detection"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_edge_detection"_h].get());
		const std::array srvs_smaa_edge_detection = { g_managed_resources.shader_resource_views["smaa_srgb_scene"_h].get(), g_managed_resources.shader_resource_views["depth"_h].get() };
		device->PSSetShaderResources(0, srvs_smaa_edge_detection.size(), srvs_smaa_edge_detection.data());

		device->ClearRenderTargetView(g_managed_resources.render_target_views["smaa_edge_detection"_h].get(), g_smaa_clear_color);
		device->ClearDepthStencilView(g_managed_resources.depth_stencil_views["smaa"_h].get(), D3D10_CLEAR_STENCIL, 1.0f, 0);
		device->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["smaa_blending_weight_calculation"_h]) {
			create_vertex_shader(device, g_managed_resources.vertex_shaders["smaa_blending_weight_calculation"_h].put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_blending_weight_calculation"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["smaa_blending_weight_calculation"_h].put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!g_managed_resources.shader_resource_views["smaa_area_tex"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = AREATEX_WIDTH;
			tex_desc.Height = AREATEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			D3D10_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = areaTexBytes;
			subresource_data.SysMemPitch = AREATEX_PITCH;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_area_tex"_h].put()), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!g_managed_resources.shader_resource_views["smaa_search_tex"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = SEARCHTEX_WIDTH;
			tex_desc.Height = SEARCHTEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			D3D10_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = searchTexBytes;
			subresource_data.SysMemPitch = SEARCHTEX_PITCH;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_search_tex"_h].put()), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_managed_resources.depth_stencils["smaa_disable_depth_use_stencil"_h]) {
			auto desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D10_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, g_managed_resources.depth_stencils["smaa_disable_depth_use_stencil"_h].put()), >= 0);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["smaa_blending_weight_calculation"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["smaa_blending_weight_calculation"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_blending_weight_calculation"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["smaa_blending_weight_calculation"_h], g_managed_resources.depth_stencil_views["smaa"_h].get());
		device->OMSetDepthStencilState(g_managed_resources.depth_stencils["smaa_disable_depth_use_stencil"_h].get(), 1);
		device->VSSetShader(g_managed_resources.vertex_shaders["smaa_blending_weight_calculation"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_blending_weight_calculation"_h].get());
		const std::array srvs_smaa_blending_weight_calculation = { g_managed_resources.shader_resource_views["smaa_edge_detection"_h].get(), g_managed_resources.shader_resource_views["smaa_area_tex"_h].get(), g_managed_resources.shader_resource_views["smaa_search_tex"_h].get() };
		device->PSSetShaderResources(0, srvs_smaa_blending_weight_calculation.size(), srvs_smaa_blending_weight_calculation.data());

		device->ClearRenderTargetView(g_managed_resources.render_target_views["smaa_blending_weight_calculation"_h].get(), g_smaa_clear_color);
		device->Draw(3, 0);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["smaa_neighborhood_blending"_h]) {
			create_vertex_shader(device, g_managed_resources.vertex_shaders["smaa_neighborhood_blending"_h].put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_neighborhood_blending"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["smaa_neighborhood_blending"_h].put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["smaa_neighborhood_blending"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["smaa_neighborhood_blending"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_neighborhood_blending"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["smaa_neighborhood_blending"_h], nullptr);
		device->VSSetShader(g_managed_resources.vertex_shaders["smaa_neighborhood_blending"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_neighborhood_blending"_h].get());
		const std::array srvs_neighborhood_blending = { srv_scene.get(), g_managed_resources.shader_resource_views["smaa_blending_weight_calculation"_h].get() };
		device->PSSetShaderResources(0, srvs_neighborhood_blending.size(), srvs_neighborhood_blending.data());

		device->Draw(3, 0);

		//

		// Tonemap_0x87A0B43D pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x87A0B43D"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_managed_resources.pixel_shaders["tonemap_0x87A0B43D"_h].put(), L"Tonemap_0x87A0B43D_ps.hlsl", "main", defines);
		}

		// Create RTs and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["tonemap_0x87A0B43D"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["tonemap_0x87A0B43D"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["tonemap_0x87A0B43D"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0x87A0B43D"_h], nullptr);
		device->VSSetShader(vs_original.get());
		device->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x87A0B43D"_h].get());
		const std::array srvs_tonemap_0x87A0B43D = { g_managed_resources.shader_resource_views["smaa_neighborhood_blending"_h].get(), g_srv_bloom_mips_y[0] };
		device->PSSetShaderResources(0, srvs_tonemap_0x87A0B43D.size(), srvs_tonemap_0x87A0B43D.data());

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		// AMD FFX CAS pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_cas"_h]) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_managed_resources.pixel_shaders["amd_ffx_cas"_h].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["amd_ffx_cas"_h]) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			Com_ptr<ID3D10Texture2D> tex;
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["amd_ffx_cas"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["amd_ffx_cas"_h], nullptr);
		device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get());
		device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0x87A0B43D"_h]);

		device->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

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
			create_pixel_shader(device, g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
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
			update_constant_buffer(g_cb.get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].get());
		device->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["amd_ffx_cas"_h]);

		device->Draw(3, 0);

		//

		return true;
	}

	return false;
}

static bool on_draw_indexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	// Here, for now we only have GTAO, so optimize with early exit.
	if (!g_gtao_enable || g_has_drawn_gtao) {
		return false;
	}

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(ps.put());
	if (!ps) {
		return false;
	}

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xB69D6558.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0xB69D6558.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xE2683E33.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0xE2683E33.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xC907CF9A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0xC907CF9A.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0x9CFB96DA.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0x9CFB96DA.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0x3B177042.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0x3B177042.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xEB7BE1D6.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0xEB7BE1D6.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_fog_0xF5E21918.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_fog_0xF5E21918.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_ceiling_debris_0xDC232D31.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ceiling_debris_0xDC232D31.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, rtv_original.put());
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_r_glass_door_0x59018C97.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_r_glass_door_0x59018C97.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_godrays_0xE689FDF8.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_godrays_0xE689FDF8.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_flashing_images_0x7862AA89.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_flashing_images_0x7862AA89.hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We may not have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_gtao(device, &rtv_original);
		g_has_drawn_gtao = true;

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
				case g_ps_fog_0xB69D6558.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0xB69D6558.guid, sizeof(g_ps_fog_0xB69D6558.hash), &g_ps_fog_0xB69D6558.hash), >= 0);
					return;
				case g_ps_fog_0xC907CF9A.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0xC907CF9A.guid, sizeof(g_ps_fog_0xC907CF9A.hash), &g_ps_fog_0xC907CF9A.hash), >= 0);
					return;
				case g_ps_godrays_0xE689FDF8.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_godrays_0xE689FDF8.guid, sizeof(g_ps_godrays_0xE689FDF8.hash), &g_ps_godrays_0xE689FDF8.hash), >= 0);
					return;
				case g_ps_fog_0x9CFB96DA.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0x9CFB96DA.guid, sizeof(g_ps_fog_0x9CFB96DA.hash), &g_ps_fog_0x9CFB96DA.hash), >= 0);
					return;
				case g_ps_fog_0x3B177042.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0x3B177042.guid, sizeof(g_ps_fog_0x3B177042.hash), &g_ps_fog_0x3B177042.hash), >= 0);
					return;
				case g_ps_fog_0xE2683E33.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0xE2683E33.guid, sizeof(g_ps_fog_0xE2683E33.hash), &g_ps_fog_0xE2683E33.hash), >= 0);
					return;
				case g_ps_fog_0xEB7BE1D6.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0xEB7BE1D6.guid, sizeof(g_ps_fog_0xEB7BE1D6.hash), &g_ps_fog_0xEB7BE1D6.hash), >= 0);
					return;
				case g_ps_r_glass_door_0x59018C97.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_r_glass_door_0x59018C97.guid, sizeof(g_ps_r_glass_door_0x59018C97.hash), &g_ps_r_glass_door_0x59018C97.hash), >= 0);
					return;
				case g_ps_fog_0xF5E21918.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_fog_0xF5E21918.guid, sizeof(g_ps_fog_0xF5E21918.hash), &g_ps_fog_0xF5E21918.hash), >= 0);
					return;
				case g_ps_ceiling_debris_0xDC232D31.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ceiling_debris_0xDC232D31.guid, sizeof(g_ps_ceiling_debris_0xDC232D31.hash), &g_ps_ceiling_debris_0xDC232D31.hash), >= 0);
					return;
				case g_ps_flashing_images_0x7862AA89.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_flashing_images_0x7862AA89.guid, sizeof(g_ps_flashing_images_0x7862AA89.hash), &g_ps_flashing_images_0x7862AA89.hash), >= 0);
					return;
				case g_ps_tonemap_0x87A0B43D.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x87A0B43D.guid, sizeof(g_ps_tonemap_0x87A0B43D.hash), &g_ps_tonemap_0x87A0B43D.hash), >= 0);
					return;
				case g_ps_bloom_downsample_0xB51C436B.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_downsample_0xB51C436B.guid, sizeof(g_ps_bloom_downsample_0xB51C436B.hash), &g_ps_bloom_downsample_0xB51C436B.hash), >= 0);
					return;
				case g_ps_bloom_0xB1DCCAE7.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xB1DCCAE7.guid, sizeof(g_ps_bloom_0xB1DCCAE7.hash), &g_ps_bloom_0xB1DCCAE7.hash), >= 0);
					return;
				case g_ps_bloom_0x1A782DB1.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x1A782DB1.guid, sizeof(g_ps_bloom_0x1A782DB1.hash), &g_ps_bloom_0x1A782DB1.hash), >= 0);
					return;
				case g_ps_bloom_0xBD24CC87.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xBD24CC87.guid, sizeof(g_ps_bloom_0xBD24CC87.hash), &g_ps_bloom_0xBD24CC87.hash), >= 0);
					return;
			}
		}
	}
}

static bool on_copy_resource(reshade::api::command_list *cmd_list, reshade::api::resource source, reshade::api::resource dest)
{
	auto resource = (ID3D10Resource*)dest.handle;
	Com_ptr<ID3D10Texture2D> tex;
	auto hr = resource->QueryInterface(tex.put());
	if (SUCCEEDED(hr)) {
		D3D10_TEXTURE2D_DESC desc;
		tex->GetDesc(&desc);
		if (desc.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS) {
			auto device = (ID3D10Device*)cmd_list->get_native();
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["depth"_h].put()), >= 0);
		}
	}
	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
			desc.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// The game uses max 4x.
		desc.max_anisotropy = 16.0f;

		return true;
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

	// Should be backbuffer.
	g_managed_resources.render_target_views["on_present"_h].reset();

	// Set the window to borderless fullscreen, the main intention.
	// Non fullscreen window with or without border can be still set from the game if first set as windowed mode.
	// Fixes UI (menus and loading screen) being pushed below lower screen border.
	// Fixes ReShade overlay being smudged/blured.
	SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
	SetWindowPos((HWND)hwnd, HWND_TOP, 0, 0, desc.back_buffer.texture.width, desc.back_buffer.texture.height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

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

	// Save device.
	g_device = (ID3D10Device*)swapchain->get_device()->get_native();

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Reset resolution dependent resources.
	//

	// GTAO
	reset_com_array(g_rtv_gtao_working_depth_mips);
	reset_com_array(g_srv_gtao_working_depth_mips);
	g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
	g_managed_resources.render_target_views["gtao_main_pass"_h].reset();
	g_managed_resources.render_target_views["gtao_denoise1_pass"_h].reset();

	// SMAA.
	g_smaa_rt_metrics.set(g_swapchain_width, g_swapchain_height);
	g_managed_resources.pixel_shaders["smaa_pre_pass"_h].reset();
	g_managed_resources.render_target_views["smaa_srgb_scene"_h].reset();
	g_managed_resources.vertex_shaders["smaa_edge_detection"_h].reset();
	g_managed_resources.pixel_shaders["smaa_edge_detection"_h].reset();
	g_managed_resources.depth_stencil_views["smaa"_h].reset();
	g_managed_resources.render_target_views["smaa_edge_detection"_h].reset();
	g_managed_resources.vertex_shaders["smaa_blending_weight_calculation"_h].reset();
	g_managed_resources.pixel_shaders["smaa_blending_weight_calculation"_h].reset();
	g_managed_resources.render_target_views["smaa_blending_weight_calculation"_h].reset();
	g_managed_resources.vertex_shaders["smaa_neighborhood_blending"_h].reset();
	g_managed_resources.pixel_shaders["smaa_neighborhood_blending"_h].reset();
	g_managed_resources.render_target_views["smaa_neighborhood_blending"_h].reset();

	// Bloom.
	reset_com_array(g_rtv_bloom_mips_y);
	reset_com_array(g_srv_bloom_mips_y);
	reset_com_array(g_rtv_bloom_mips_x);
	reset_com_array(g_srv_bloom_mips_x);

	g_managed_resources.render_target_views["tonemap_0x87A0B43D"_h].reset();
	g_managed_resources.render_target_views["amd_ffx_cas"_h].reset();

	//
}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	// Set maximum frame latency to 1.
	auto native_device = (ID3D10Device*)device->get_native();
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
	g_cb.reset();
	g_managed_resources.clear();

	release_com_array(g_rtv_gtao_working_depth_mips);
	release_com_array(g_srv_gtao_working_depth_mips);
	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable)) {
		reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
	}
	if (!reshade::get_config_value(nullptr, NAME, "GTAOFOVY", g_gtao_fov_y)) {
		reshade::set_config_value(nullptr, NAME, "GTAOFOVY", g_gtao_fov_y);
	}
	if (!reshade::get_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality)) {
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
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
	if (!reshade::get_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}

	if (!reshade::get_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);

	if (!reshade::get_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
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
	ImGui::Spacing();

	if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1.0, 10.0)) {
		g_bloom_sigmas.resize(g_bloom_nmips);
		release_com_array(g_rtv_bloom_mips_y);
		release_com_array(g_srv_bloom_mips_y);
		release_com_array(g_rtv_bloom_mips_x);
		release_com_array(g_srv_bloom_mips_x);
		g_rtv_bloom_mips_x.resize(g_bloom_nmips);
		g_srv_bloom_mips_x.resize(g_bloom_nmips);
		g_rtv_bloom_mips_y.resize(g_bloom_nmips);
		g_srv_bloom_mips_y.resize(g_bloom_nmips);
	}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	}

	ImGui::NewLine();
	#endif

	if (ImGui::Checkbox("GTAO enable", &g_gtao_enable)) {
		if (!g_gtao_enable) {
			g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h].reset();
			g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h].reset();
			g_managed_resources.shader_resource_views["gtao_working_depth"_h].reset();
			g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
			g_managed_resources.render_target_views["gtao_main_pass"_h].reset();
			g_managed_resources.shader_resource_views["gtao_main_pass"_h].reset();
			g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h].reset();
			g_managed_resources.render_target_views["gtao_denoise1_pass"_h].reset();
			g_managed_resources.shader_resource_views["gtao_denoise1_pass"_h].reset();
			g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h].reset();
			g_managed_resources.blends["gtao"_h].reset();
			reset_com_array(g_rtv_gtao_working_depth_mips);
			reset_com_array(g_srv_gtao_working_depth_mips);
		}
		reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_gtao_enable);
	}
	ImGui::BeginDisabled(!g_gtao_enable);
	ImGui::InputFloat("GTAO FOV Y", &g_gtao_fov_y);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_gtao_fov_y = std::clamp(g_gtao_fov_y, 0.0f, 360.0f);
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOFOVY", g_gtao_fov_y);
	}
	static constexpr std::array gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("GTAO Quality", &g_gtao_quality, gtao_quality_items.data(), gtao_quality_items.size())) {
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom Intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["tonemap_0x87A0B43D"_h].reset();
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
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
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
		reshade::set_config_value(nullptr, NAME, "TRC", g_trc);
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
		reshade::set_config_value(nullptr, NAME, "Gamma", g_gamma);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, NAME, "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, NAME, "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
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
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			assert(g_bloom_nmips == 6);
			g_bloom_sigmas[0] = 1.5f;
			g_bloom_sigmas[1] = 2.0f;
			g_bloom_sigmas[2] = 2.0f;
			g_bloom_sigmas[3] = 2.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::finish_present>(on_finish_present);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::copy_resource>(on_copy_resource);
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
