#include "Common.h"
#include "Helpers.h"

#define DEV 0

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

static bool on_resolve_texture_region(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
{
	// If we let the game do the resolve it will use a wrong format after we made RT format upgrades.
	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	ctx->ResolveSubresource((ID3D11Resource*)dest.handle, dest_subresource, (ID3D11Resource*)source.handle, source_subresource, DXGI_FORMAT_R16G16B16A16_UNORM);
	return true;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Upgrade render targets.
	if (((desc.usage & reshade::api::resource_usage::render_target) != 0)) {

		#if DEV
		log_debug("RT: {}", to_string(desc.texture.format));
		#endif

		if (desc.texture.format == reshade::api::format::r8g8b8a8_typeless) {	
			desc.texture.format = reshade::api::format::r16g16b16a16_typeless;
			return true;
		}
	}

	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	// Try to filter only render targets that we have upgraded.
	auto resource_desc = device->get_resource_desc(resource);
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		// Back buffer.
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_typeless) {

			#if DEV
			log_debug("Original view into r8g8b8a8_typeless: {}", to_string(desc.format));
			#endif

			desc.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.back_buffer_count = 2;
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.sync_interval = 0;
	return true;
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(IID_PPV_ARGS(&device1));
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "GrisGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "GrisGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / static_cast<double>(g_user_set_fps_limit));
	if (!reshade::get_config_value(nullptr, "GrisGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "GrisGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>(static_cast<double>(g_user_set_accounted_error) / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 1.0f, 1000.0f);
		reshade::set_config_value(nullptr, "GrisGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / static_cast<double>(g_user_set_fps_limit));
	}
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "GrisGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>(static_cast<double>(g_user_set_accounted_error) / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "GrisGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "GrisGraphicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/GrisGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			read_config();
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
