#include <reshade.hpp>
#include <dxgi1_6.h>

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	if (desc.back_buffer_count < 2) {
		desc.back_buffer_count = 2;
	}
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// Disable vsync in windowed mode.
	// Also present have to be called with DXGI_PRESENT_ALLOW_TEARING flag,
	// which reshade should handle.
	desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	
	desc.fullscreen_state = false;

	// Force max screen refresh rate.
	desc.fullscreen_refresh_rate = 0.0f;

	desc.sync_interval = 0;

	bool force_10bit_format = false;
	if (!reshade::get_config_value(nullptr, "UpgradeSwapEffect", "Force10BitFormat", force_10bit_format)) {
		reshade::set_config_value(nullptr, "UpgradeSwapEffect", "Force10BitFormat", "0");
	}
	if (force_10bit_format) {
		desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	}

	// _srgb formats are not supported when using flip swap effects.
	else if (desc.back_buffer.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb) {
		desc.back_buffer.texture.format = reshade::api::format::r8g8b8a8_unorm;
	}
	else if (desc.back_buffer.texture.format == reshade::api::format::b8g8r8a8_unorm_srgb) {
		desc.back_buffer.texture.format = reshade::api::format::b8g8r8a8_unorm;
	}

	return true;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		return true;
	}
	return false;
}

extern "C" __declspec(dllexport) const char *NAME = "UpgradeSwapEffect";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Upgrade swapchain to flip model and enable tearing.";
extern "C" __declspec(dllexport) const char *WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/UpgradeSwapEffect";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
