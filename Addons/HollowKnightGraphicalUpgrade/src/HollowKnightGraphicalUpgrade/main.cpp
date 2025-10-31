#include "Common.h"
#include "Helpers.h"

#define DEV 0

// Generate noise/grain
constexpr uint32_t g_ps_0xF1B9A141_hash = 0xF1B9A141;
static uintptr_t g_ps_0xF1B9A141;

// Dividing desired noise update rate with 3 matches the general FPS limit for whatever reason.
static int g_user_set_noise_update_rate = 60; // in (desired) updates per second
static std::chrono::high_resolution_clock::duration g_noise_update_interval;
static bool g_update_noise;

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	g_update_noise = false;
}

static bool on_draw_indexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	
	// We are using PS as hash for draw calls.
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(&ps, nullptr, nullptr);
	if (!ps) {
		return false;
	}

	// 0xF1B9A141 is drawn multiple times (always 3 times?) per frame.
	if ((uintptr_t)ps.get() == g_ps_0xF1B9A141) {
		// Limit noise update rate.
		const auto now = std::chrono::high_resolution_clock::now();
		static auto last_update = now;
		if (now - last_update >= g_noise_update_interval) {
			last_update += g_noise_update_interval;
			g_update_noise = true;
		}
		if (g_update_noise) {
			return false;
		}
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
			if (hash == g_ps_0xF1B9A141_hash) {
				g_ps_0xF1B9A141 = pipeline.handle;
			}
		}
	}
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs. 
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (desc.texture.format == reshade::api::format::r8g8b8a8_typeless) {
			desc.texture.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		// Back buffer.
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_unorm) {

			#if DEV
			// We only expect _unorm.
			if (desc.format != reshade::api::format::r8g8b8a8_unorm) {
				log_debug("View into rgba8_typless was: {}!", to_string(desc.format));
			}
			#endif

			desc.format = reshade::api::format::r16g16b16a16_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	return true;
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(&device1);
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "HollowKnightGraphicalUpgrade", "NoiseUpdateRate", g_user_set_noise_update_rate)) {
		reshade::set_config_value(nullptr, "HollowKnightGraphicalUpgrade", "NoiseUpdateRate", g_user_set_noise_update_rate);
	}
	g_noise_update_interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / ((double)g_user_set_noise_update_rate / 3.0)));
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	ImGui::InputInt("Noise update rate", &g_user_set_noise_update_rate);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_noise_update_rate = std::clamp(g_user_set_noise_update_rate, 1, 1000);
		reshade::set_config_value(nullptr, "HollowKnightGraphicalUpgrade", "NoiseUpdateRate", g_user_set_noise_update_rate);
		g_noise_update_interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / ((double)g_user_set_noise_update_rate / 3.0)));
	}
}

extern "C" __declspec(dllexport) const char* NAME = "HollowKnightGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "HollowKnightGraphicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/HollowKnightGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			read_config();
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
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
