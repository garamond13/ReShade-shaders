#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"

extern "C" __declspec(dllexport) const char* NAME = "FarCry2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v3.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/FarCry2GraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_linearize_depth_0xC2E9EBB5 = { 0xC2E9EBB5, { 0x38db79e5, 0xa0ed, 0x48af, { 0x9d, 0x24, 0xa6, 0x2e, 0x7f, 0xcb, 0x1f, 0x44 }}};
constexpr Shader_hash g_ps_linearize_depth_msaa2x_0x61D074F7 = { 0x61D074F7, { 0x243e41c3, 0xa264, 0x4361, { 0x9b, 0xc4, 0x1e, 0x7e, 0x95, 0x2f, 0x68, 0xa8 }}};
constexpr Shader_hash g_ps_linearize_depth_msaa4x_0x7E5AFFEC = { 0x7E5AFFEC, { 0xadb54af1, 0x6555, 0x4d42, { 0x92, 0xd9, 0xe3, 0xb, 0xc4, 0xab, 0x73, 0xcd }}};
constexpr Shader_hash g_ps_linearize_depth_msaa8x_0x4B33AF55 = { 0x4B33AF55, { 0x16474a22, 0xa716, 0x49cb, { 0xab, 0xdc, 0x47, 0x33, 0x14, 0xb3, 0xd9, 0x2e }}};

constexpr Shader_hash g_ps_msaa2x_resolve_0x99D2B96F = { 0x99D2B96F, { 0xc1559822, 0x98b7, 0x4f61, { 0x9b, 0xdd, 0x5b, 0x25, 0x83, 0x35, 0xaf, 0x4c }}};
constexpr Shader_hash g_ps_msaa4x_resolve_0x7E8792FA = { 0x7E8792FA, { 0x96a94287, 0xdae9, 0x4a61, { 0xa0, 0xb3, 0x6e, 0x78, 0xae, 0xd6, 0x1f, 0xb3 }}};
constexpr Shader_hash g_ps_msaa8x_resolve_0x8D5108F3 = { 0x8D5108F3, { 0xa4d8bb8b, 0x6ead, 0x44cb, { 0xa1, 0x7b, 0x40, 0xf0, 0xdb, 0x4e, 0xd7, 0x44 }}};

constexpr Shader_hash g_ps_road_0x5D206A73 = { 0x5D206A73, { 0x2f324504, 0x6411, 0x496b, { 0xa5, 0xe5, 0xc3, 0xe3, 0x3c, 0x21, 0x94, 0x1 }}};
constexpr Shader_hash g_ps_bloom_0xC5143189 = { 0xC5143189, { 0x252aef3b, 0xbf1a, 0x4b09, { 0x81, 0xd0, 0x2, 0xa2, 0xb3, 0xb2, 0x77, 0x87 }}};
constexpr Shader_hash g_ps_downsample_0x4E1CD411 = { 0x4E1CD411, { 0xfda2019e, 0xa676, 0x4fdf, { 0xb2, 0x53, 0xa8, 0xe, 0x98, 0x14, 0x9f, 0x4b }}};
constexpr Shader_hash g_ps_bloom_upsample_0x2DE809A9 = { 0x2DE809A9, { 0xe51d2ccb, 0x2213, 0x4ba8, { 0xa3, 0xa2, 0xfc, 0xb1, 0x66, 0x39, 0x43, 0xc }}};
constexpr Shader_hash g_ps_tonemap_0x8B2AB983 = { 0x8B2AB983, { 0x7e409b3d, 0x210, 0x4edd, { 0x9d, 0x97, 0x71, 0x5f, 0xb9, 0x9d, 0x21, 0xc }}};

//

static ID3D10Device* g_device;
static Managed_resources g_managed_resources;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static float g_amd_ffx_cas_sharpness = 0.0f;
static float g_bloom_intensity = 1.0;

constexpr int GTAO_DEPTH_MIP_LEVELS = 5;
static bool g_enable_gtao = true;
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
static bool g_draw_gtao;

static std::array<ID3D10RenderTargetView*, GTAO_DEPTH_MIP_LEVELS> g_rtv_gtao_working_depth_mips;
static std::array<ID3D10ShaderResourceView*, GTAO_DEPTH_MIP_LEVELS> g_srv_gtao_working_depth_mips;

static void draw_gtao(ID3D10RenderTargetView*const* rtv)
{
	// Backup.
	//

	// Primitive topology.
	D3D10_PRIMITIVE_TOPOLOGY primitive_topology_original;
	g_device->IAGetPrimitiveTopology(&primitive_topology_original);

	// VS.
	Com_ptr<ID3D10VertexShader> vs_original;
	g_device->VSGetShader(vs_original.put());

	// Viewports.
	UINT nviewports;
	g_device->RSGetViewports(&nviewports, nullptr);
	std::vector<D3D10_VIEWPORT> viewports_original(nviewports);
	g_device->RSGetViewports(&nviewports, viewports_original.data());

	// Rasterizer.
	Com_ptr<ID3D10RasterizerState> rasterizer_original;
	g_device->RSGetState(rasterizer_original.put());

	// PS.
	Com_ptr<ID3D10PixelShader> ps_original;
	g_device->PSGetShader(ps_original.put());
	std::array<ID3D10ShaderResourceView*, 2> srvs_original = {};
	g_device->PSGetShaderResources(0, srvs_original.size(), srvs_original.data());
	Com_ptr<ID3D10SamplerState> smp_original;
	g_device->PSGetSamplers(0, 1, smp_original.put());

	// Blend.
	Com_ptr<ID3D10BlendState> blend_original;
	FLOAT blend_factor_original[4];
	UINT sample_mask_original;
	g_device->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

	// RTs.
	Com_ptr<ID3D10RenderTargetView> rtv_original;
	Com_ptr<ID3D10DepthStencilView> dsv_original;
	g_device->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());

	//

	// PrefilterDepths passes.
	//

	// Create and bind Fullscreen Triangle VS.
	[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
		create_vertex_shader(g_device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
	}

	// Create prefilter depths PS, for mip0.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h]) {
		create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths_mip0_ps");
	}

	// Create prefilter depths PS, for mips 1 to 4.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h]) {
		create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths_ps");
	}

	// Create point clamp sampler.
	[[unlikely]] if (!g_managed_resources.samplers["point_clamp"_h]) {
		create_sampler_point_clamp(g_device, g_managed_resources.samplers["point_clamp"_h].put());
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
		ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

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
			ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_gtao_working_depth_mips[i]), >= 0);
			srv_desc.Texture2D.MostDetailedMip = i;
			ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_gtao_working_depth_mips[i]), >= 0);
		}

		// Create working depth SRV with all mips.
		ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_working_depth"_h].put()), >= 0);
	}

	// Prefilter depths viewport, for mip0.
	D3D10_VIEWPORT viewport = {};
	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Bindings.
	g_device->OMSetRenderTargets(1, &g_rtv_gtao_working_depth_mips[0], nullptr);
	g_device->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
	g_device->RSSetState(nullptr);
	g_device->RSSetViewports(1, &viewport);
	g_device->PSSetShader(g_managed_resources.pixel_shaders["gtao_prefilter_depths_mip0"_h].get());
	g_device->PSSetConstantBuffers(12, 1, &g_managed_resources.buffers["linearize_depth_cb0"_h]);
	g_device->PSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
	g_device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["linearized_depth"_h]);
	g_device->OMSetBlendState(nullptr, nullptr, UINT_MAX);

	// Prefilter depths, mip0.
	g_device->Draw(3, 0);

	// Bindings.
	g_device->PSSetShader(g_managed_resources.pixel_shaders["gtao_prefilter_depths"_h].get());

	// Prefilter depths, mips 1 to GTAO_DEPTH_MIP_LEVELS.
	for (int i = 1; i < GTAO_DEPTH_MIP_LEVELS; ++i) {
		viewport.Width = std::max(1, g_swapchain_width >> i);
		viewport.Height = std::max(1, g_swapchain_height >> i);

		// Bindings.
		g_device->OMSetRenderTargets(1, &g_rtv_gtao_working_depth_mips[i], nullptr);
		g_device->RSSetViewports(1, &viewport);
		g_device->PSSetShaderResources(0, 1, &g_srv_gtao_working_depth_mips[i - 1]);

		g_device->Draw(3, 0);
	}

	//

	// MainPass pass.
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_main_pass"_h]) {
		const std::string quality_str = std::to_string(g_gtao_quality);
		D3D_SHADER_MACRO defines[] = {
			{ "GTAO_QUALITY", quality_str.c_str() },
			{ nullptr, nullptr }
		};
		create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_main_pass"_h].put(), L"GTAO_impl.hlsl", "main_pass_ps", defines);
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
		ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["gtao_main_pass"_h].put()), >= 0);
		ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_main_pass"_h].put()), >= 0);
	}

	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Bindings.
	g_device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["gtao_main_pass"_h], nullptr);
	g_device->RSSetViewports(1, &viewport);
	g_device->PSSetShader(g_managed_resources.pixel_shaders["gtao_main_pass"_h].get());
	g_device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_working_depth"_h]);

	g_device->Draw(3, 0);

	//

	// Doing 2 XeGTAO denoise passes correspond to "Denoising level: Medium" from the XeGTAO demo.

	// DenoisePass1 pass.
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h]) {
		create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_ps");
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
		ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["gtao_denoise1_pass"_h].put()), >= 0);
		ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_denoise1_pass"_h].put()), >= 0);
	}

	// Bindings.
	g_device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["gtao_denoise1_pass"_h], nullptr);
	g_device->PSSetShader(g_managed_resources.pixel_shaders["gtao_denoise1_pass"_h].get());
	g_device->PSSetShaderResources(1, 1, &g_managed_resources.shader_resource_views["gtao_main_pass"_h]);

	g_device->Draw(3, 0);

	//

	// DenoisePass2 pass
	//

	// Create PS.
	[[unlikely]] if (!g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h]) {
		const D3D_SHADER_MACRO defines[] = {
			{ "FINAL_APPLY", "" },
			{ nullptr, nullptr }
		};
		create_pixel_shader(g_device, g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_ps", defines);
	}

	// Create blend.
	[[unlikely]] if (!g_managed_resources.blends["gtao"_h]) {
		auto blend_desc = default_D3D10_BLEND_DESC();
		blend_desc.BlendEnable[0] = TRUE;
		blend_desc.SrcBlend = D3D10_BLEND_DEST_COLOR;
		ensure(g_device->CreateBlendState(&blend_desc, g_managed_resources.blends["gtao"_h].put()), >= 0);
	}

	// Bindings.
	////

	g_device->OMSetRenderTargets(1, rtv, nullptr);
	g_device->PSSetShader(g_managed_resources.pixel_shaders["gtao_denoise2_pass"_h].get());
	g_device->PSSetShaderResources(1, 1, &g_managed_resources.shader_resource_views["gtao_denoise1_pass"_h]);

	#if !(DEV && SHOW_AO)
	g_device->OMSetBlendState(g_managed_resources.blends["gtao"_h].get(), nullptr, UINT_MAX);
	#endif

	////

	g_device->Draw(3, 0);

	//

	// Restore states.
	g_device->OMSetRenderTargets(1, &rtv_original, dsv_original.get());
	g_device->IASetPrimitiveTopology(primitive_topology_original);
	g_device->VSSetShader(vs_original.get());
	g_device->RSSetViewports(nviewports, viewports_original.data());
	g_device->RSSetState(rasterizer_original.get());
	g_device->PSSetShader(ps_original.get());
	g_device->PSSetShaderResources(0, srvs_original.size(), srvs_original.data());
	g_device->PSSetSamplers(0, 1, &smp_original);
	g_device->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

	release_com_array(srvs_original);
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

	#if DEV
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	auto on_linearize_depth = [&]() {
		if (g_enable_gtao) {
			g_draw_gtao = true;
			device->PSGetConstantBuffers(0, 1, g_managed_resources.buffers["linearize_depth_cb0"_h].put()); 

			// Create linearized depth SRV.
			Com_ptr<ID3D10RenderTargetView> rtv;
			device->OMGetRenderTargets(1, rtv.put(), nullptr);
			Com_ptr<ID3D10Resource> resource;
			rtv->GetResource(resource.put());
			ensure(device->CreateShaderResourceView(resource.get(), nullptr, g_managed_resources.shader_resource_views["linearized_depth"_h].put()), >= 0);
		}
	};

	auto on_msaa_resolve = [&]() {
		if (g_draw_gtao) {
			Com_ptr<ID3D10ShaderResourceView> srv;
			device->PSGetShaderResources(0, 1, srv.put());
			Com_ptr<ID3D10Resource> resource;
			srv->GetResource(resource.put());
			Com_ptr<ID3D10RenderTargetView> rtv;
			ensure(device->CreateRenderTargetView(resource.get(), nullptr, rtv.put()), >= 0);
			draw_gtao(&rtv);
			g_draw_gtao = false;
		}
	};

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_linearize_depth_0xC2E9EBB5.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_linearize_depth_0xC2E9EBB5.hash) {
		on_linearize_depth();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_linearize_depth_msaa2x_0x61D074F7.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_linearize_depth_msaa2x_0x61D074F7.hash) {
		on_linearize_depth();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_linearize_depth_msaa4x_0x7E5AFFEC.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_linearize_depth_msaa4x_0x7E5AFFEC.hash) {
		on_linearize_depth();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_linearize_depth_msaa8x_0x4B33AF55.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_linearize_depth_msaa8x_0x4B33AF55.hash) {
		on_linearize_depth();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_msaa2x_resolve_0x99D2B96F.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_msaa2x_resolve_0x99D2B96F.hash) {
		on_msaa_resolve();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_msaa4x_resolve_0x7E8792FA.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_msaa4x_resolve_0x7E8792FA.hash) {
		on_msaa_resolve();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_msaa8x_resolve_0x8D5108F3.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_msaa8x_resolve_0x8D5108F3.hash) {
		on_msaa_resolve();
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xC5143189.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xC5143189.hash) {
		if (g_draw_gtao) {
			Com_ptr<ID3D10ShaderResourceView> srv;
			device->PSGetShaderResources(0, 1, srv.put());
			Com_ptr<ID3D10Resource> resource;
			srv->GetResource(resource.put());
			Com_ptr<ID3D10RenderTargetView> rtv;
			ensure(device->CreateRenderTargetView(resource.get(), nullptr, rtv.put()), >= 0);
			draw_gtao(&rtv);
			g_draw_gtao = false;
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_0xC5143189"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["bloom_0xC5143189"_h].put(), L"Bloom_0xC5143189_ps.hlsl");
		}

		// Bindings.
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_0xC5143189"_h].get());

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_downsample_0x4E1CD411.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_downsample_0x4E1CD411.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["downsample_0x4E1CD411"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["downsample_0x4E1CD411"_h].put(), L"Downsample_0x4E1CD411_ps.hlsl");
		}

		// Bindings.
		device->PSSetShader(g_managed_resources.pixel_shaders["downsample_0x4E1CD411"_h].get());

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_upsample_0x2DE809A9.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_upsample_0x2DE809A9.hash) {
		// SRV0 is the largest MIP among SRVs (MIP1, RTV is MIP0), SRV3 is the smallest MIP (MIP4).
		std::array<ID3D10ShaderResourceView*, 4> srvs;
		device->PSGetShaderResources(0, srvs.size(), srvs.data());
		
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_upsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_upsample"_h].put(), L"Bloom_upsample_0x2DE809A9_ps.hlsl");
		}

		// Create RTVs.
		// RTV0 will be MIP0 and RTV3 will be MIP3.
		Com_ptr<ID3D10Resource> resource;
		std::array<ID3D10RenderTargetView*, 4> rtvs;
		device->OMGetRenderTargets(1, &rtvs[0], nullptr);
		for (size_t i = 0; i < 3; ++i) {
			srvs[i]->GetResource(resource.put());
			ensure(device->CreateRenderTargetView(resource.get(), nullptr, &rtvs[i + 1]), >= 0);
		}

		// Bindings.
		device->PSSetShader(g_managed_resources.pixel_shaders["bloom_upsample"_h].get());

		// The blend mode is:
		// src D3D10_BLEND_ONE
		// dst D3D10_BLEND_ONE
		// op D3D10_BLEND_OP_ADD

		D3D10_VIEWPORT viewport = {};
		for (int i = 3; i >= 0; --i) {
			// Get RT texture description.
			rtvs[i]->GetResource(resource.put());
			Com_ptr<ID3D10Texture2D> tex;
			ensure(resource->QueryInterface(tex.put()), >= 0);
			D3D10_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			viewport.Width = tex_desc.Width;
			viewport.Height = tex_desc.Height;

			// Bindings.
			device->OMSetRenderTargets(1, &rtvs[i], nullptr);
			device->RSSetViewports(1, &viewport);
			device->PSSetShaderResources(0, 1, &srvs[i]);
		
			cmd_list->draw(vertex_count, instance_count, first_instance, first_instance);
		}

		release_com_array(srvs);
		release_com_array(rtvs);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x8B2AB983.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x8B2AB983.hash) {
		// Backup RTV.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// tonemap_0x8B2AB983 pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x8B2AB983"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_managed_resources.pixel_shaders["tonemap_0x8B2AB983"_h].put(), L"Tonemap_0x8B2AB983_ps.hlsl", "main", defines);
		}

		// Create RT views.
		[[unlikely]] if (!g_managed_resources.render_target_views["tonemap_0x8B2AB983"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["tonemap_0x8B2AB983"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["tonemap_0x8B2AB983"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0x8B2AB983"_h], nullptr);
		device->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x8B2AB983"_h].get());

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

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get());
		device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0x8B2AB983"_h]);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

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

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(ps.put());
	if (!ps) {
		return false;
	}

	#if DEV
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	[[unlikely]] if (g_draw_gtao) {
		Com_ptr<ID3D10RenderTargetView> rtv;
		device->OMGetRenderTargets(1, rtv.put(), nullptr);
		draw_gtao(&rtv);
		g_draw_gtao = false;
		return false;
	}

	// Skiping this fixes AO drawing shadows on roads.
	// Also skiping this doesn't seam to have any negative visual impact.
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_road_0x5D206A73.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_road_0x5D206A73.hash) {
		return true;
	}

	return false;
}

// This in debug build is causing game to crash before the first intro video! 
static bool on_copy_resource(reshade::api::command_list* cmd_list, reshade::api::resource source, reshade::api::resource dest)
{
	// If MSAA is off the game should replace a MSAA resolve PS with the copy (if MSAA resolve PS would run in the first place),
	// right after the linearize depth pass.
	if (g_draw_gtao) {
		auto device = (ID3D10Device*)cmd_list->get_native();
		auto resource = (ID3D10Resource*)source.handle;
		Com_ptr<ID3D10RenderTargetView> rtv;
		ensure(device->CreateRenderTargetView(resource, nullptr, rtv.put()), >= 0);
		draw_gtao(&rtv);
		g_draw_gtao = false;
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
				case g_ps_road_0x5D206A73.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_road_0x5D206A73.guid, sizeof(g_ps_road_0x5D206A73.hash), &g_ps_road_0x5D206A73.hash), >= 0);
					return;
				case g_ps_linearize_depth_0xC2E9EBB5.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_linearize_depth_0xC2E9EBB5.guid, sizeof(g_ps_linearize_depth_0xC2E9EBB5.hash), &g_ps_linearize_depth_0xC2E9EBB5.hash), >= 0);
					return;
				case g_ps_linearize_depth_msaa2x_0x61D074F7.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_linearize_depth_msaa2x_0x61D074F7.guid, sizeof(g_ps_linearize_depth_msaa2x_0x61D074F7.hash), &g_ps_linearize_depth_msaa2x_0x61D074F7.hash), >= 0);
					return;
				case g_ps_linearize_depth_msaa4x_0x7E5AFFEC.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_linearize_depth_msaa4x_0x7E5AFFEC.guid, sizeof(g_ps_linearize_depth_msaa4x_0x7E5AFFEC.hash), &g_ps_linearize_depth_msaa4x_0x7E5AFFEC.hash), >= 0);
					return;
				case g_ps_linearize_depth_msaa8x_0x4B33AF55.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_linearize_depth_msaa8x_0x4B33AF55.guid, sizeof(g_ps_linearize_depth_msaa8x_0x4B33AF55.hash), &g_ps_linearize_depth_msaa8x_0x4B33AF55.hash), >= 0);
					return;
				case g_ps_msaa2x_resolve_0x99D2B96F.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_msaa2x_resolve_0x99D2B96F.guid, sizeof(g_ps_msaa2x_resolve_0x99D2B96F.hash), &g_ps_msaa2x_resolve_0x99D2B96F.hash), >= 0);
					return;
				case g_ps_msaa4x_resolve_0x7E8792FA.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_msaa4x_resolve_0x7E8792FA.guid, sizeof(g_ps_msaa4x_resolve_0x7E8792FA.hash), &g_ps_msaa4x_resolve_0x7E8792FA.hash), >= 0);
					return;
				case g_ps_msaa8x_resolve_0x8D5108F3.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_msaa8x_resolve_0x8D5108F3.guid, sizeof(g_ps_msaa8x_resolve_0x8D5108F3.hash), &g_ps_msaa8x_resolve_0x8D5108F3.hash), >= 0);
					return;
				case g_ps_bloom_0xC5143189.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xC5143189.guid, sizeof(g_ps_bloom_0xC5143189.hash), &g_ps_bloom_0xC5143189.hash), >= 0);
					return;
				case g_ps_downsample_0x4E1CD411.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_downsample_0x4E1CD411.guid, sizeof(g_ps_downsample_0x4E1CD411.hash), &g_ps_downsample_0x4E1CD411.hash), >= 0);
					return;
				case g_ps_bloom_upsample_0x2DE809A9.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_upsample_0x2DE809A9.guid, sizeof(g_ps_bloom_upsample_0x2DE809A9.hash), &g_ps_bloom_upsample_0x2DE809A9.hash), >= 0);
					return;
				case g_ps_tonemap_0x8B2AB983.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x8B2AB983.guid, sizeof(g_ps_tonemap_0x8B2AB983.hash), &g_ps_tonemap_0x8B2AB983.hash), >= 0);
					return;
			}
		}
	}
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
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		desc.max_anisotropy = 16.0f;
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
	g_device = (ID3D10Device*)swapchain->get_device()->get_native();

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Reset resolution dependentresources.
	//

	// GTAO.
	reset_com_array(g_rtv_gtao_working_depth_mips);
	reset_com_array(g_srv_gtao_working_depth_mips);
	g_managed_resources.render_target_views["gtao_main_pass"_h].reset();
	g_managed_resources.render_target_views["gtao_denoise1_pass"_h].reset();

	g_managed_resources.render_target_views["tonemap_0x8B2AB983"_h].reset();

	//
}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	// The game is on device creating spree.
	// Filter out anything thats not D3D10.
	if (device->get_api() != reshade::api::device_api::d3d10) {
		return;
	}

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
	g_managed_resources.clear();
	reset_com_array(g_rtv_gtao_working_depth_mips);
	reset_com_array(g_srv_gtao_working_depth_mips);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "GTAOEnable", g_enable_gtao)) {
		reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_enable_gtao);
	}
	if (!reshade::get_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality)) {
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
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

	if (ImGui::Checkbox("GTAO enable", &g_enable_gtao)) {
		if (!g_enable_gtao) {
			g_managed_resources.buffers["linearize_depth_cb0"_h].reset();
			g_managed_resources.shader_resource_views["linearized_depth"_h].reset();
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
		reshade::set_config_value(nullptr, NAME, "GTAOEnable", g_enable_gtao);
	}
	ImGui::BeginDisabled(!g_enable_gtao);
	static constexpr std::array gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("GTAO Quality", &g_gtao_quality, gtao_quality_items.data(), gtao_quality_items.size())) {
		g_managed_resources.pixel_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
		g_managed_resources.pixel_shaders["amd_ffx_cas"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
		g_managed_resources.pixel_shaders["tonemap_0x8B2AB983"_h].reset();
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
			reshade::register_event<reshade::addon_event::copy_resource>(on_copy_resource);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
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
