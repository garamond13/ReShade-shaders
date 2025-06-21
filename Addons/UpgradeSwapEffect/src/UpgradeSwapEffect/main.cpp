#include <reshade.hpp>
#include <dxgi1_2.h>
#include <MinHook.h>

// IDXGISwapChain::Present
typedef HRESULT(__stdcall *Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static Present present = nullptr;

// IDXGISwapChain1::Present1
typedef HRESULT(__stdcall *Present1)(IDXGISwapChain1* swapchain, UINT sync_interval, UINT present_flags, const DXGI_PRESENT_PARAMETERS* present_params);
static Present1 present1 = nullptr;

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	flags |= DXGI_PRESENT_ALLOW_TEARING;
	return present(swapchain, sync_interval, flags);
}

static HRESULT __stdcall detour_present1(IDXGISwapChain1* swapchain, UINT sync_interval, UINT present_flags, const DXGI_PRESENT_PARAMETERS* present_params)
{
	present_flags |= DXGI_PRESENT_ALLOW_TEARING;
	return present1(swapchain, sync_interval, present_flags, present_params);
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	if (desc.back_buffer_count < 2) {
		desc.back_buffer_count = 2;
	}
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	// Required for DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING flag.
	desc.sync_interval = 0;
	desc.fullscreen_state = false;

	bool force_10bit_format = false;
	if (!reshade::get_config_value(nullptr, "APP", "Force10BitFormat", force_10bit_format)) {
		reshade::set_config_value(nullptr, "APP", "Force10BitFormat", "0");
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
// Required for DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING flag.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		return true;
	}
	return false;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	static bool hook_installed = false;
	if (!hook_installed) {
		auto native_swapchain = reinterpret_cast<IDXGISwapChain*>(swapchain->get_native());
		auto vtable = *reinterpret_cast<void***>(native_swapchain);

		// Try to hook IDXGISwapChain::Present
		present = reinterpret_cast<Present>(vtable[8]);
		if (MH_CreateHook(vtable[8], &detour_present, reinterpret_cast<void**>(&present)) == MH_OK && MH_EnableHook(vtable[8]) == MH_OK) {
			reshade::log::message(reshade::log::level::info, "Hooked IDXGISwapChain::Present");

			// Try to hook IDXGISwapChain1::Present1
			IDXGISwapChain1* swapchain1 = nullptr;
			if (SUCCEEDED(native_swapchain->QueryInterface(&swapchain1))) {
				present1 = reinterpret_cast<Present1>(vtable[22]);
				if (MH_CreateHook(vtable[22], &detour_present1, reinterpret_cast<void**>(&present1)) == MH_OK && MH_EnableHook(vtable[22]) == MH_OK) {
					reshade::log::message(reshade::log::level::info, "Hooked IDXGISwapChain1::Present1");
					hook_installed = true;
				}
				swapchain1->Release();
			}

			// This is ok. We only have hooked IDXGISwapChain::Present
			else {
				hook_installed = true;
			}
		}
	}
}

extern "C" __declspec(dllexport) const char *NAME = "Upgrade Swap Effect";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Upgrade swapchain to flip model.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			MH_Uninitialize();
			break;
	}
	return TRUE;
}
