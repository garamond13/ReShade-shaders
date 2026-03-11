#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Helpers.h"
#include "TRC.h"
#include "BlueNoiseTex.h"

struct alignas(16) CB_data
{
   float tex_noise_index;
};

// Shader hooks.
//

constexpr uint32_t g_ps_tonemap_0x87C347A4_hash = 0x87C347A4;
constexpr GUID g_ps_tonemap_0x87C347A4_guid = { 0x702cd243, 0xfd79, 0x4b77, { 0xb5, 0x95, 0xf7, 0x35, 0x4a, 0x52, 0xbe, 0xa5 } };

//

static CB_data g_cb_data;
static uint32_t g_swapchain_width;
static uint32_t g_swapchain_height;
static bool g_force_vsync_off = true;
static float g_bloom_intensity = 1.0f;
static float g_amd_ffx_cas_sharpness = 0.4f;
static float g_amd_ffx_lfga_amount = 0.4f;

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

static std::unordered_map<uint32_t, Com_ptr<ID3D11RenderTargetView>> g_rtv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11ShaderResourceView>> g_srv;
static std::unordered_map<uint32_t, Com_ptr<ID3D11VertexShader>> g_vs;
static std::unordered_map<uint32_t, Com_ptr<ID3D11PixelShader>> g_ps;
static std::unordered_map<uint32_t, Com_ptr<ID3D11Buffer>> g_cb;

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
	auto hr = ps->GetPrivateData(g_ps_tonemap_0x87C347A4_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x87C347A4_hash) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// RTV should be the back buffer, unless scaling is used.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Tonemap 0x87C347A4 pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps[hash_name("tonemap_0x87C347A4")]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps[hash_name("tonemap_0x87C347A4")].put(), L"Tonemap_0x87C347A4_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		[[unlikely]] if (!g_rtv[hash_name("tonemap_0x87C347A4")]) {
			// Create texture.
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

			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv[hash_name("tonemap_0x87C347A4")].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv[hash_name("tonemap_0x87C347A4")].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv[hash_name("tonemap_0x87C347A4")], nullptr);
		ctx->PSSetShader(g_ps[hash_name("tonemap_0x87C347A4")].get(), nullptr, 0);

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

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
			// Create texture.
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

			ensure(device->CreateRenderTargetView(tex.get(), nullptr, g_rtv[hash_name("amd_ffx_cas")].put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv[hash_name("amd_ffx_cas")].put()), >= 0);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv[hash_name("amd_ffx_cas")], nullptr);
		ctx->VSSetShader(g_vs[hash_name("fullscreen_triangle")].get(), nullptr, 0);
		ctx->PSSetShader(g_ps[hash_name("amd_ffx_cas")].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv[hash_name("tonemap_0x87C347A4")]);

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

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_ps_tonemap_0x87C347A4_hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x87C347A4_guid, sizeof(g_ps_tonemap_0x87C347A4_hash), &g_ps_tonemap_0x87C347A4_hash), >= 0);
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

	// Reset resolution dependent resources.
	g_rtv[hash_name("tonemap_0x87C347A4")].reset();
	g_srv[hash_name("tonemap_0x87C347A4")].reset();
	g_rtv[hash_name("amd_ffx_cas")].reset();
	g_srv[hash_name("amd_ffx_cas")].reset();
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
	g_srv.clear();
	g_vs.clear();
	g_ps.clear();
	g_cb.clear();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "AfterimageGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	#if DEV
	if (ImGui::Button("Dev button")) {
	}
	ImGui::Spacing();
	#endif

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps[hash_name("tonemap_0x87C347A4")].reset();;
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps[hash_name("amd_ffx_cas")].reset();
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "TRC", g_trc);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "Gamma", g_gamma);
		g_ps[hash_name("amd_ffx_lfga")].reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "AfterimageGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "AfterimageGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "AfterimageGraphicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/AfterimageGraphicalUpgrade";

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
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
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