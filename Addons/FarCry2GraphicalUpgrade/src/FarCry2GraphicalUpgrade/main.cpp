#include "Common.h"
#include "Helpers.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

// Do we have a LUT.CUBE file in the GraphicalUpgrade folder.
static bool g_has_lut;

// All the replacement shader code.
static thread_local std::vector<Com_ptr<ID3D10Blob>> g_shader_code;

// The last post process before UI and the malaria attack effect.
// RT should be the back buffer unless we are having malaria attack.
static uintptr_t g_ps_0xDBF8FCBD;

// Tone map before grain.
static uintptr_t g_ps_0x8B2AB983;

static Com_ptr<ID3D10VertexShader> g_vs_fullscreen_triangle;
static Com_ptr<ID3D10PixelShader> g_ps_srgb_to_linear;

// AMD FFX CAS
static Com_ptr<ID3D10PixelShader> g_ps_amd_ffx_cas;
static float g_amd_ffx_cas_sharpness = 0.4f;

// LUT
static size_t g_lut_size;
static Com_ptr<ID3D10ShaderResourceView> g_srv_lut;
static Com_ptr<ID3D10PixelShader> g_ps_sample_lut;

static std::filesystem::path get_shaders_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, MAX_PATH);
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	return path;
}

// Returns true if we replaced the original code, otherwise returns false.
static bool replace_shader_code(reshade::api::shader_desc* desc)
{
	if (desc->code_size == 0) {
		return false;
	}

	// Make shader file path from shader hash and check do we have a replacement shader on that path.
	const uint32_t hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
	const std::filesystem::path path = g_graphical_upgrade_path / std::format(L"0x{:08X}.hlsl", hash);
	if (!std::filesystem::exists(path)) {
		return false;
	}

	// Try to compile the shader file.
	g_shader_code.push_back(nullptr);
	Com_ptr<ID3D10Blob> error;
	auto hr = D3DCompileFromFile(path.c_str(), nullptr, nullptr, "main", "ps_4_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &g_shader_code.back(), &error);
	if (FAILED(hr)) {
		if (error) {
			log_error("D3DCompileFromFile: {}", (const char*)error->GetBufferPointer());
		}
		else {
			log_error("D3DCompileFromFile: (HRESULT){:08X}", hr);
		}
		return false;
	}

	// Replace the original shader code with the compiled replacement shader code.
	desc->code = g_shader_code.back()->GetBufferPointer();
	desc->code_size = g_shader_code.back()->GetBufferSize();
	
	return true;
}

static bool lut_exists()
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= L"LUT.CUBE";
	if (std::filesystem::exists(path)) {
		return true;
	}
	return false;
}

// FIXME: Optimize this without turnning it into assembly level of readable.
static void read_cube_file(std::vector<float>& data, size_t& lut_size)
{
	const std::filesystem::path path = g_graphical_upgrade_path / L"LUT.CUBE";
	std::ifstream file(path);
	std::string line;
	bool is_header = true;
	size_t i = 0;
	while (std::getline(file, line)) {
		// Skip empty lines and comments.
		[[unlikely]] if (line.empty() || line[0] == '#') {
			continue;
		}

		// Parse header.
		[[unlikely]] if (is_header) {
			if (line.rfind("TITLE", 0) == 0) {
				continue;
			}
			if (line.rfind("LUT_1D_SIZE", 0) == 0) {
				log_error("LUT is 1D! Only 3D LUTs are supported.");
				return;
			}
			if (line.rfind("LUT_3D_SIZE", 0) == 0) {
				std::sscanf(line.c_str(), "LUT_3D_SIZE %u", &lut_size);
				data.resize(lut_size * lut_size * lut_size * 4);
				continue;
			}

			// We only expect (support) 0.0 0.0 0.0 and 1.0 1.0 1.0.
			// TODO: Handle properly DOMAIN_MAX less than 1.0?
			// TODO: Show errors on domains outside of [0.0, 1.0]?
			if (line.rfind("DOMAIN_MIN", 0) == 0) {
				continue;
			}
			if (line.rfind("DOMAIN_MAX", 0) == 0) {
				continue;
			}

			// Not an empty line, comment or a keyword.
			// Assume start of the data table.
			is_header = false;
		}

		// Parse the data table.
		float r, g, b;
		std::sscanf(line.c_str(), "%f %f %f", &r, &g, &b);
		data[i++] = r;
		data[i++] = g;
		data[i++] = b;
		i++; // Skip alpha.
	}
}

static void create_srv_lut(ID3D10Device* device, ID3D10ShaderResourceView** srv)
{
	std::vector<float> data;
	read_cube_file(data, g_lut_size);
	D3D10_TEXTURE3D_DESC tex_desc = {};
	tex_desc.Width = g_lut_size;
	tex_desc.Height = g_lut_size;
	tex_desc.Depth = g_lut_size;
	tex_desc.MipLevels = 1;

	// This should be better supported than DXGI_FORMAT_R32G32B32_FLOAT,
	// performance should also be equivalent.
	tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	D3D10_SUBRESOURCE_DATA subresource_data = {};
	subresource_data.pSysMem = data.data();
	subresource_data.SysMemPitch = g_lut_size * 4 * sizeof(float); // width * nchannals * bytedepth
	subresource_data.SysMemSlicePitch = g_lut_size * g_lut_size * 4 * sizeof(float); // width * height * nchannals * bytedepth
	Com_ptr<ID3D10Texture3D> tex;
	ensure(device->CreateTexture3D(&tex_desc, &subresource_data, &tex), >= 0);
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv), >= 0);
}

static void compile_shader(ID3DBlob** code, const wchar_t* file, const char* target, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	std::filesystem::path path = g_graphical_upgrade_path;
	path /= file;
	Com_ptr<ID3DBlob> error;
	auto hr = D3DCompileFromFile(path.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code, &error);
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
	D3DDisassemble((*code)->GetBufferPointer(), (*code)->GetBufferSize(), 0, nullptr, &disassembly);
	std::ofstream assembly(path.replace_filename(path.filename().string() + "_" + entry_point + ".asm"));
	assembly.write((const char*)disassembly->GetBufferPointer(), disassembly->GetBufferSize());
	#endif
}

static void create_vertex_shader(ID3D10Device* device, ID3D10VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "vs_4_1", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), vs), >= 0);
}

static void create_pixel_shader(ID3D10Device* device, ID3D10PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "ps_4_1", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), ps), >= 0);
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	auto device = (ID3D10Device*)cmd_list->get_native();

	// We are using PS as hash for draw calls.
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(&ps);
	if (!ps) {
		return false;
	}
	const auto hash = (uintptr_t)ps.get();

	if (hash == g_ps_0x8B2AB983) {

		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// Maybe we should skip all this and just make some assumptions?
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Render the original shader 0x8B2AB983 to texture.
		//

		RT_views rt_views_0x8B2AB983(device, tex_desc);

		// Bindings.
		device->OMSetRenderTargets(1, rt_views_0x8B2AB983.get_rtv_address(), nullptr);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		#if DEV
		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(1, 1, &smp);
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// sRGBToLinear pass
		//

		// Setup IA for fullscreen triangle draw.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology_original;
		device->IAGetPrimitiveTopology(&primitive_topology_original);
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Create Fullscreen Triangle VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_srgb_to_linear) {
			create_pixel_shader(device, &g_ps_srgb_to_linear, L"sRGBToLinear_ps.hlsl");
		}

		RT_views rt_views_srgb_to_linear(device, tex_desc);

		// Bindings
		device->OMSetRenderTargets(1, rt_views_srgb_to_linear.get_rtv_address(), nullptr);
		device->VSSetShader(g_vs_fullscreen_triangle.get());
		device->PSSetShader(g_ps_srgb_to_linear.get());
		device->PSSetShaderResources(0, 1, rt_views_0x8B2AB983.get_srv_address());

		device->Draw(3, 0);

		//

		// AMD FFX CAS pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_amd_ffx_cas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D10_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_amd_ffx_cas, L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->PSSetShaderResources(0, 1, rt_views_srgb_to_linear.get_srv_address());
		device->PSSetShader(g_ps_amd_ffx_cas.get());

		device->Draw(3, 0);

		//

		// Restore original states.
		device->IASetPrimitiveTopology(primitive_topology_original);

		return true;
	}

	// The malaria attack effect 0x12A5247D gets rendered after this,
	// so apply our post proccess to it as well?
	if (g_has_lut && hash == g_ps_0xDBF8FCBD) {

		// We expect RTV to be a back buffer, unless we are having malaria attack.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// Maybe we should skip all this and just make some assumptions?
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Render the original shader 0xDBF8FCBD to texture.
		//

		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		tex_desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
		RT_views rt_views_0xDBF8FCBD(device, tex_desc);

		// Bindings.
		device->OMSetRenderTargets(1, rt_views_0xDBF8FCBD.get_rtv_address(), nullptr);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		// SampleLUT pass
		//

		// Setup IA for fullscreen triangle draw.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology_original;
		device->IAGetPrimitiveTopology(&primitive_topology_original);
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Create Fullscreen Triangle VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create SRV LUT.
		[[unlikely]] if (!g_srv_lut) {
			create_srv_lut(device, &g_srv_lut);
		}

		// Create PS.
		[[unlikely]] if (!g_ps_sample_lut) {
			const std::string lut_size_str = std::to_string(g_lut_size);
			const D3D10_SHADER_MACRO defines[] = {
				{ "LUT_SIZE", lut_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_sample_lut, L"SampleLUT_ps.hlsl", "main", defines);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->VSSetShader(g_vs_fullscreen_triangle.get());
		device->PSSetShader(g_ps_sample_lut.get());
		const std::array ps_resources_sample_lut = { rt_views_0xDBF8FCBD.get_srv(), g_srv_lut.get() };
		device->PSSetShaderResources(0, ps_resources_sample_lut.size(), ps_resources_sample_lut.data());

		device->Draw(3, 0);

		//

		// Restore original states.
		device->IASetPrimitiveTopology(primitive_topology_original);

		return true;
	}

	return false;
}

static bool on_create_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects)
{
	// Go through all shader stages that are in this pipeline and replace shaders for which we have a replacement.
	bool replaced_any = false;
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			replaced_any |= replace_shader_code((reshade::api::shader_desc*)subobjects[i].data);
		}
	}
	return replaced_any;
}

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case 0xDBF8FCBD:
					g_ps_0xDBF8FCBD = pipeline.handle;
					break;
				case 0x8B2AB983:
					g_ps_0x8B2AB983 = pipeline.handle;
					break;
			}
		}
	}

	// Free the memory allocated in the "replace_shader_code" call.
	g_shader_code.clear();
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Upgrade depth stencil.
	if (desc.texture.format == reshade::api::format::r24_g8_typeless) {
		desc.texture.format = reshade::api::format::r32_g8_typeless;
		return true;
	}

	// Upgrade render targets.
	if (((desc.usage & reshade::api::resource_usage::render_target) != 0)) {
		if (desc.texture.format == reshade::api::format::r8g8b8a8_unorm) {
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

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	const auto resource_desc = device->get_resource_desc(resource);

	// Depth stencil.
	if (resource_desc.texture.format == reshade::api::format::r32_g8_typeless) {
		if (desc.format == reshade::api::format::d24_unorm_s8_uint) {
			desc.format = reshade::api::format::d32_float_s8_uint;
			return true;
		}
		if (desc.format == reshade::api::format::r24_unorm_x8_uint) {
			desc.format = reshade::api::format::r32_float_x8_uint;
			return true;
		}
	}

	// Render targets.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
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

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	// The game already uses anisotropic filtering where it matters.
	desc.max_anisotropy = 16.0f;

	return true;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// Always force the game into borderless window.
	// The game if not forced into borderless window or fullscreen exclusive mode
	// may start with wrong window size or wrong swapchain size.
	SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
	SetWindowPos((HWND)hwnd, HWND_TOP, 0, 0, desc.back_buffer.texture.width, desc.back_buffer.texture.height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_state = false;
	desc.sync_interval = 0;
	return true;
}

static void on_init_device(reshade::api::device* device)
{
	// The game is on device creating spree.
	// Filter out anything thats not D3D10.
	if (device->get_api() != reshade::api::device_api::d3d10) {
		return;
	}

	// Set maximum frame latency to 1, the game is not setting this already to 1.
	// It reduces input latecy massivly if GPU bound.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(&device1);
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "FarCry2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "FarCry2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "FarCry2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps_amd_ffx_cas.reset();
	}
}

extern "C" __declspec(dllexport) const char* NAME = "FarCry2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "FarCry2GraphicalUpgrade v2.0.1";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/FarCry2GraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			g_has_lut = lut_exists();
			read_config();
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
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
