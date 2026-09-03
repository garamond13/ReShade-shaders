#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"

extern "C" __declspec(dllexport) const char* NAME = "AfterimageGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v2.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/AfterimageGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_tonemap_0x87C347A4 = { 0x87C347A4, { 0x116435be, 0xd2f1, 0x4c0d, { 0xa1, 0x3, 0x13, 0xf9, 0x0, 0x44, 0xfa, 0xf3 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D11Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static float g_bloom_intensity = 1.0f;
static float g_amd_ffx_cas_sharpness = 0.0f;
static float g_amd_ffx_lfga_amount = 0.3f;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

static void on_finish_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain)
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
	hr = ps->GetPrivateData(g_ps_tonemap_0x87C347A4.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x87C347A4.hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// RTV should be the back buffer, unless scaling is used.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Tonemap 0x87C347A4 pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x87C347A4"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_managed_resources.pixel_shaders["tonemap_0x87C347A4"_h].put(), L"Tonemap_0x87C347A4_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["tonemap_0x87C347A4"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["tonemap_0x87C347A4"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["tonemap_0x87C347A4"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["tonemap_0x87C347A4"_h], nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x87C347A4"_h].get(), nullptr, 0);

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		// AMD FFX CAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(device.get(), g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_cas"_h]) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_managed_resources.pixel_shaders["amd_ffx_cas"_h].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["amd_ffx_cas"_h]) {
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
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["amd_ffx_cas"_h].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["amd_ffx_cas"_h].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["amd_ffx_cas"_h], nullptr);
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["tonemap_0x87C347A4"_h]);

		ctx->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_lfga"_h]) {
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const D3D_SHADER_MACRO defines[] = {
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
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
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));
			last_update += interval;
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["amd_ffx_cas"_h]);

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
				case g_ps_tonemap_0x87C347A4.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x87C347A4.guid, sizeof(g_ps_tonemap_0x87C347A4.hash), &g_ps_tonemap_0x87C347A4.hash), >= 0);
					return;
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
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
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

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// The game is not always using 16x.
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

}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	// Set maximum frame latency to 1.
	auto native_device = (ID3D11Device*)device->get_native();
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
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount);
	}
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

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
		g_managed_resources.pixel_shaders["tonemap_0x87C347A4"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
		g_managed_resources.pixel_shaders["amd_ffx_cas"_h].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "GrainAmount", g_amd_ffx_lfga_amount);
		g_managed_resources.pixel_shaders["amd_ffx_lfga"_h].reset();
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
			reshade::register_event<reshade::addon_event::finish_present>(on_finish_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
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
