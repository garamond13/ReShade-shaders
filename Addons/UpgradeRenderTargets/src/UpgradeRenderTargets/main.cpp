#include <reshade.hpp>

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	if (desc.type == reshade::api::resource_type::texture_2d && (desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::unordered_access)) != 0) {
		switch (desc.texture.format) {
			case reshade::api::format::r8g8b8a8_unorm:
			case reshade::api::format::r8g8b8a8_unorm_srgb:
			case reshade::api::format::r8g8b8x8_unorm:
			case reshade::api::format::r8g8b8x8_unorm_srgb:
			case reshade::api::format::b8g8r8a8_unorm:
			case reshade::api::format::b8g8r8a8_unorm_srgb:
			case reshade::api::format::b8g8r8x8_unorm:
			case reshade::api::format::b8g8r8x8_unorm_srgb:
			case reshade::api::format::r10g10b10a2_unorm:
				desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
				return true;
			case reshade::api::format::r11g11b10_float:
				desc.texture.format = reshade::api::format::r16g16b16a16_float;
				return true;
		}
	}
	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	// Try to filter only render targets that we have upgraded.
	const auto resource_desc = device->get_resource_desc(resource);
	if (resource_desc.type == reshade::api::resource_type::texture_2d && (resource_desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::unordered_access)) != 0) {
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

extern "C" __declspec(dllexport) const char *NAME = "UpgradeRenderTargets";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Upgrade render targets to higher precision formats.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule)) {
			return FALSE;
		}
		reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
		reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}
	return TRUE;
}
