#include "Common.h"
#include "Helpers.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0

// Tone responce curve.
enum TRC_
{
	TRC_SRGB,
	TRC_GAMMA
};
typedef int TRC;

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

// Shader hooks and replacemnt shaders.
//

// Tonemap should be the first draw into the backbuffer.
constexpr GUID g_backbuffer_guid = { 0x7bc25cd7, 0xe813, 0x4307, { 0xa2, 0x7d, 0x4c, 0xf5, 0xf5, 0x9, 0x56, 0xfd } };
static bool g_has_drawn_tonemap;

//

static Com_ptr<ID3D11VertexShader> g_vs_fullscreen_triangle;
static Com_ptr<ID3D11PixelShader> g_ps_linearize;
static bool g_force_vsync_off = true;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// AMD FFX RCAS
static Com_ptr<ID3D11PixelShader> g_ps_amd_ffx_rcas;
static float g_amd_ffx_rcas_sharpness = 1.0f;

// FPS limiter.
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

static std::filesystem::path get_shaders_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	return path;
}

static void compile_shader(ID3DBlob** code, const wchar_t* file, const char* target, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= file;
	Com_ptr<ID3DBlob> error;
	auto hr = D3DCompileFromFile(path.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code, error.put());
	if (FAILED(hr)) {
		if (error) {
			log_error("D3DCompileFromFile: {}", (const char*)error->GetBufferPointer());
		}
		else {
			log_error("D3DCompileFromFile: (HRESULT){:08X}", hr);
		}
	}

	#if DEV && OUTPUT_ASSEMBLY
	Com_ptr<ID3DBlob> disassembly;
	D3DDisassemble((*code)->GetBufferPointer(), (*code)->GetBufferSize(), 0, nullptr, disassembly.put());
	std::ofstream assembly(path.replace_filename(path.filename().string() + "_" + entry_point + ".asm"));
	assembly.write((const char*)disassembly->GetBufferPointer(), disassembly->GetBufferSize());
	#endif
}

static void create_vertex_shader(ID3D11Device* device, ID3D11VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "vs_5_0", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vs), >= 0);
}

static void create_pixel_shader(ID3D11Device* device, ID3D11PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "ps_5_0", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, ps), >= 0);
}

static void create_compute_shader(ID3D11Device* device, ID3D11ComputeShader** cs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "cs_5_0", entry_point, defines);
	ensure(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, cs), >= 0);
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	g_has_drawn_tonemap = false;

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

	// We are looking for the backbuffer.
	// It should be (tonemap) the last draw before UI.
	//
	// Tonemap shader has too many permutations,
	// so it's inconvinient to look for it by PS.
	Com_ptr<ID3D11RenderTargetView> rtv;
	ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
	if (!rtv) {
		return false;
	}
	Com_ptr<ID3D11Resource> resource;
	rtv->GetResource(resource.put());

	uint8_t data;
	UINT size = sizeof(data);
	if (!g_has_drawn_tonemap && SUCCEEDED(resource->GetPrivateData(g_backbuffer_guid, &size, &data))) {	
		// Get backbuffer teture description.
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		// Tonemap (original shader) pass.
		//

		// Create views.
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_tonemap;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_tonemap.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_tonemap;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_tonemap.put()), >= 0);
		
		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_tonemap, nullptr);

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		// Linearize pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), g_vs_fullscreen_triangle.put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_linearize) {
			create_pixel_shader(device.get(), g_ps_linearize.put(), L"Linearize_ps.hlsl");
		}

		// Create views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_linearize;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_linearize.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_linearize;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_linearize.put()), >= 0);

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_linearize, nullptr);
		ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_linearize.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_tonemap);

		ctx->Draw(3, 0);

		//

		// AMD FFX RCAS pass
		//

		[[unlikely]] if (!g_ps_amd_ffx_rcas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_rcas_sharpness);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), g_ps_amd_ffx_rcas.put(), L"AMD_FFX_RCAS_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv, nullptr);
		ctx->PSSetShader(g_ps_amd_ffx_rcas.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_linearize);

		ctx->Draw(3, 0);

		//

		g_has_drawn_tonemap = true;
		
		return true;
	}

	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		// Back buffer.
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_typeless) {
			if (desc.format == reshade::api::format::r10g10b10a2_unorm) {
				desc.format = reshade::api::format::r16g16b16a16_unorm;
				return true;
			}
			#if DEV
			else {
				log_debug("View for r16g16b16a16_typeless was {}!", to_string(desc.format));
			}
			#endif
		}
		if (resource_desc.texture.format == reshade::api::format::r32_typeless) {
			if (desc.format == reshade::api::format::r16_float) {
				desc.format = reshade::api::format::r32_float;
				return true;
			}
			#if DEV
			else {
				log_debug("View for r32_typeless was {}!", to_string(desc.format));
			}
			#endif
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	// The game is creating rgba8_typeless resources and is creating _srgb views, but are _srgb views ever used?
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r10g10b10a2_typeless) {
			desc.texture.format = reshade::api::format::r16g16b16a16_typeless;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16_typeless) {
			desc.texture.format = reshade::api::format::r32_typeless;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	desc.max_anisotropy = 16.0f;
	return true;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	if (g_force_vsync_off) {
		desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		desc.sync_interval = 0;
	}

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	Com_ptr<ID3D11Resource> backbuffer;
	ensure(native_swapchain->GetBuffer(0, IID_PPV_ARGS(backbuffer.put())), >= 0);
	uint8_t data = 0; // Irelevant, but necessary.
	ensure(backbuffer->SetPrivateData(g_backbuffer_guid, sizeof(data), &data), >= 0);
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
	g_vs_fullscreen_triangle.reset();
	g_ps_linearize.reset();
	g_ps_amd_ffx_rcas.reset();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "INSIDEGraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "INSIDEGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "INSIDEGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (!reshade::get_config_value(nullptr, "INSIDEGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "INSIDEGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_rcas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "Sharpness", g_amd_ffx_rcas_sharpness);
		g_ps_amd_ffx_rcas.reset();
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
		g_ps_amd_ffx_rcas.reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
		g_ps_amd_ffx_rcas.reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();

	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 20.0f, FLT_MAX);
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}

	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "INSIDEGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "INSIDEGrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "INSIDEGrapicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/INSIDEGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
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