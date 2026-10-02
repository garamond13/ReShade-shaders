#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "SMAA/SMAA.h"

extern "C" __declspec(dllexport) const char* NAME = "CrysisGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/CrysisGraphicalUpgrade";

static ID3D10Device* g_device;
static Managed_resources g_managed_resources;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static bool g_has_msaa;

// SMAA
static SMAA_rt_metrics g_smaa_rt_metrics;

// Shader hooks.
//

// Shader quality, high or very high.
constexpr Shader_hash g_ps_tonemap_0xD95EBBAB = { 0xD95EBBAB, { 0xc180b288, 0x4698, 0x458f, { 0xac, 0xf6, 0x69, 0xe1, 0x44, 0xaa, 0xe2, 0x40 }}};

//

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	// The game may try to change window style, happens during loading screen.
	// We have to change back to borderless window.
	if (g_force_modern_windowed) {
		auto exstyle = GetWindowLongPtr((HWND)swapchain->get_hwnd(), GWL_EXSTYLE);
		[[unlikely]] if (exstyle) {
			SetWindowLongPtr((HWND)swapchain->get_hwnd(), GWL_STYLE, WS_POPUP);
			SetWindowLongPtr((HWND)swapchain->get_hwnd(), GWL_EXSTYLE, 0);
			SetWindowPos((HWND)swapchain->get_hwnd(), HWND_TOP, 0, 0, g_swapchain_width, g_swapchain_height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
	}

	// We have to rebind RTV and DSV after present for `DXGI_SWAP_EFFECT_FLIP_DISCARD` to work properly in this game.
	g_device->OMGetRenderTargets(1, g_managed_resources.render_target_views["on_present"_h].put(), g_managed_resources.depth_stencil_views["on_present"_h].put());
}

static void on_finish_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain)
{
	g_device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["on_present"_h], g_managed_resources.depth_stencil_views["on_present"_h].get());

	// FPS limiter
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
	hr = ps->GetPrivateData(g_ps_tonemap_0xD95EBBAB.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xD95EBBAB.hash) {
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		D3D10_PRIMITIVE_TOPOLOGY primitive_topology;
		device->IAGetPrimitiveTopology(&primitive_topology);

		// Tonemap_0xD95EBBAB pass
		//

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["tonemap_0xD95EBBAB"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["tonemap_0xD95EBBAB"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["tonemap_0xD95EBBAB"_h].put()), >= 0);
		}

		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0xD95EBBAB"_h], nullptr);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		#if DEV
		// Sampler in slot 0 should be:
		// D3D10_FILTER_MIN_MAG_MIP_POINT
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD 100
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(0, 1, smp.put());
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_POINT || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != 100.0f || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD 100
		// D3D10_COMPARISON_NEVER
		device->PSGetSamplers(1, 1, smp.put());
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != 100.0f || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Resolve depth pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["resolve_depth"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["resolve_depth"_h].put(), L"ResolveDepth_ps.hlsl");
		}

		if (g_managed_resources.shader_resource_views["multisampled_depth"_h]) {
			// Create RT and views.
			[[unlikely]] if (!g_managed_resources.render_target_views["depth"_h]) {
				D3D10_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
				Com_ptr<ID3D10Texture2D> tex;
				ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["depth"_h].put()), >= 0);
				ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["depth"_h].put()), >= 0);
			}

			// Bindings.
			device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["depth"_h], nullptr);
			device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
			device->PSSetShader(g_managed_resources.pixel_shaders["resolve_depth"_h].get());
			device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["multisampled_depth"_h]);

			device->Draw(3, 0);
		}

		//

		// SMAAPrePass pass
		//
		// Linearize scene
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["smaa_pre_pass"_h]) {
			create_pixel_shader(device, g_managed_resources.pixel_shaders["smaa_pre_pass"_h].put(), L"SMAA_impl.hlsl", "smaa_pre_pass_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["smaa_pre_pass"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["smaa_pre_pass"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["smaa_pre_pass"_h].put()), >= 0);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &g_managed_resources.render_target_views["smaa_pre_pass"_h], nullptr);
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		device->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_pre_pass"_h].get());
		device->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0xD95EBBAB"_h]);

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
		const std::array srvs_smaa_edge_detection = { g_managed_resources.shader_resource_views["tonemap_0xD95EBBAB"_h].get(), g_managed_resources.shader_resource_views["depth"_h].get() };
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

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->VSSetShader(g_managed_resources.vertex_shaders["smaa_neighborhood_blending"_h].get());
		device->PSSetShader(g_managed_resources.pixel_shaders["smaa_neighborhood_blending"_h].get());
		const std::array srvs_neighborhood_blending = { g_managed_resources.shader_resource_views["smaa_pre_pass"_h].get(), g_managed_resources.shader_resource_views["smaa_blending_weight_calculation"_h].get() };
		device->PSSetShaderResources(0, srvs_neighborhood_blending.size(), srvs_neighborhood_blending.data());

		device->Draw(3, 0);

		//

		// Restore.
		device->IASetPrimitiveTopology(primitive_topology);

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
				case g_ps_tonemap_0xD95EBBAB.hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xD95EBBAB.guid, sizeof(g_ps_tonemap_0xD95EBBAB.hash), &g_ps_tonemap_0xD95EBBAB.hash), >= 0);
					return;
			}
		}
	}
}

static bool on_resolve_texture_region(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
{
	// If we let the game do the resolve it will use a wrong format after we made RT upgrades of the format rg16_float.
	if (format == reshade::api::format::r16g16_float) {
		cmd_list->resolve_texture_region(source, source_subresource, source_box, dest, dest_subresource, dest_x, dest_y, dest_z, reshade::api::format::r32g32_float);
		return true;
	}
	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r32g32_float) {
			desc.format = reshade::api::format::r32g32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		// Depth, only when using MSAA.
		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}
	}

	// Depth buffer has no SRV bind flag, so we need to add it.
	if (desc.texture.format == reshade::api::format::d24_unorm_s8_uint && desc.texture.width == g_swapchain_width && desc.texture.height == g_swapchain_height) {
		desc.texture.format = reshade::api::format::r24_g8_typeless;
		desc.usage |= reshade::api::resource_usage::shader_resource;
		return true;
	}

	return false;
}

static void on_init_resource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state, reshade::api::resource resource)
{
	// Create depth SRV.
	auto native_resource = (ID3D10Resource*)resource.handle;
	Com_ptr<ID3D10Texture2D> tex;
	auto hr = native_resource->QueryInterface(tex.put());
	if (SUCCEEDED(hr)) {
		D3D10_TEXTURE2D_DESC desc;
		tex->GetDesc(&desc);
		if (desc.Format == DXGI_FORMAT_R24G8_TYPELESS) {
			D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			if (desc.SampleDesc.Count > 1) {
				srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
				ensure(((ID3D10Device*)device->get_native())->CreateShaderResourceView(tex.get(), &srv_desc, g_managed_resources.shader_resource_views["multisampled_depth"_h].put()), >= 0);
				g_managed_resources.shader_resource_views["depth"_h].reset(); // For if we turn on MSAA in game that was previously off.
			}
			else {
				srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = 1;
				ensure(((ID3D10Device*)device->get_native())->CreateShaderResourceView(tex.get(), &srv_desc, g_managed_resources.shader_resource_views["depth"_h].put()), >= 0);
				g_managed_resources.shader_resource_views["multisampled_depth"_h].reset(); // For if we turn off MSAA in game that was previously on.
			}
		}
	}
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// The game is not already using 16x.
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

	// Release backbuffer.
	g_managed_resources.render_target_views["on_present"_h].reset();

	if (g_force_modern_windowed) {
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

	// Reset reolution dependent resources.
	//

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

	g_managed_resources.render_target_views["depth"_h].reset();

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
	g_managed_resources.clear();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
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
	ImGui::NewLine();

	#endif

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
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::finish_present>(on_finish_present);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
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
