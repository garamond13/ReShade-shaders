#include <imgui.h>
#include <reshade.hpp>
#include <chrono>
#include <thread>
#include <timeapi.h>
#include <algorithm>

// Exposed to user.
static float g_user_set_fps_limit = 60.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static double g_frame_interval; // in seconds
static double g_accounted_error; // in seconds

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	static std::chrono::high_resolution_clock::time_point start;

	// We need to account for the acctual frame time.
	const double sleep_time = g_frame_interval - std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();

	// Precise sleep.
	const auto sleep_start = std::chrono::high_resolution_clock::now();
	timeBeginPeriod(1);
	std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time - g_accounted_error));
	timeEndPeriod(1);
	while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - sleep_start).count() < sleep_time) {
		continue;
	}

	start = std::chrono::high_resolution_clock::now();
}

static void draw_settings(reshade::api::effect_runtime* runtime)
{
	// Set frame interval.
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 1.0f, 1000.0f);
		reshade::set_config_value(nullptr, "AccurateFPSLimiter", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = 1.0 / static_cast<double>(g_user_set_fps_limit);
	}
	
	// Set accounted error.
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "AccurateFPSLimiter", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = static_cast<double>(g_user_set_accounted_error) / 1000.0;
	}
}

extern "C" __declspec(dllexport) const char *NAME = "AccurateFPSLimiter";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "An accurate FPS limiter";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}

			// Get/set FPS limit from config.
			if (!reshade::get_config_value(nullptr, "AccurateFPSLimiter", "FPSLimit", g_user_set_fps_limit)) {
				reshade::set_config_value(nullptr, "AccurateFPSLimiter", "FPSLimit", g_user_set_fps_limit);
			}
			g_frame_interval = 1.0 / static_cast<double>(g_user_set_fps_limit);

			// Get/set accounted error from config.
			if (!reshade::get_config_value(nullptr, "AccurateFPSLimiter", "AccountedError", g_user_set_accounted_error)) {
				reshade::set_config_value(nullptr, "AccurateFPSLimiter", "AccountedError", g_user_set_accounted_error);
			}
			g_accounted_error = static_cast<double>(g_user_set_accounted_error) / 1000.0;
			
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_overlay(nullptr, draw_settings);
			break;
		}
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
		}
	return TRUE;
}
