#include <reshade.hpp>

static bool on_create_swapchain(reshade::api::swapchain_desc &desc, void *)
{
	desc.back_buffer_count = 2;
	desc.present_mode = 4; // DXGI_SWAP_EFFECT_FLIP_DISCARD
	return true;
}

extern "C" __declspec(dllexport) const char *NAME = "Upgrade Swap Effect";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Upgrade swap effect to DXGI_SWAP_EFFECT_FLIP_DISCARD.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
		{
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			reshade::unregister_addon(hModule);
			break;
		}
	}
	return TRUE;
}
