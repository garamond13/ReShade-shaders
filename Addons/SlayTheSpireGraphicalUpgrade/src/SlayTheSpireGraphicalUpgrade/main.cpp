#include "Common.h"
#include "Helpers.h"

#define DEV 0

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// The game has to be set to windowed mode for reshade to work at all.
	//
	// Set window to borderless fullscren mode if desktop resolution is requested.
	//
	// If monitor resolution is requested, window nor swapchain will have the correct resolution (it will be slightly smaller).
	// If smaller than monitor resolution is requested, window and swapchain will have the correct resolution.

	// Get the nearest to the window monitor and it's info.
	HMONITOR hmonitor = MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXW monitorinfo = { sizeof(MONITORINFOEXW) };
	ensure(GetMonitorInfoW(hmonitor, &monitorinfo), != 0);

	// Check does user wants to run the game in windowed mode.
	bool windowed = false;
	DEVMODEW devmode = { sizeof(DEVMODEW) };
	for (DWORD i = 0; EnumDisplaySettingsW(monitorinfo.szDevice, i, &devmode); ++i) {
		if (desc.back_buffer.texture.width == devmode.dmPelsWidth && desc.back_buffer.texture.height == devmode.dmPelsHeight) {
			windowed = true;
			break;
		}
	}

	// If user doesn't want to run the game in windowed mode set it to borderless fullscreen.
	if (!windowed) {
		const auto x = monitorinfo.rcMonitor.left;
		const auto y = monitorinfo.rcMonitor.top;
		const auto w = monitorinfo.rcMonitor.right - monitorinfo.rcMonitor.left;
		const auto h = monitorinfo.rcMonitor.bottom - monitorinfo.rcMonitor.top;
		SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
		SetWindowPos((HWND)hwnd, HWND_TOP, x, y, w, h, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}

	return false;
}

extern "C" __declspec(dllexport) const char* NAME = "SlayTheSpireGrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "SlayTheSpireGrapicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/SlayTheSpireGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
