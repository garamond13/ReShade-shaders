#include "Common.h"
#include "Helpers.h"
#include "AreaTex.h"
#include "SearchTex.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0

// Tone responce curve.
enum TRC_
{
	TRC_SRGB,
	TRC_GAMMA
};

// We need to pass this to all XeGTAO PSes on resolution change or XeGTAO user settings change.
class XeGTAO_defines
{
public:

	void set(float width, float height, float fov_y, float radius, float slice_count)
	{
		// Values to strings.
		const float tan_half_fov_y = std::tan(fov_y * std::numbers::pi_v<float> / 180.0f * 0.5f); // radians = degrees * pi / 180.
		const float tan_half_fov_x = tan_half_fov_y * width / height;
		viewport_pixel_size_str = std::format("float2({},{})", 1.0f / width, 1.0f / height);
		ndc_to_view_mul_str = std::format("float2({},{})", tan_half_fov_x * 2.0f, tan_half_fov_y * -2.0f);
		ndc_to_view_add_str = std::format("float2({},{})", -tan_half_fov_x, tan_half_fov_y);
		radius_str = std::to_string(radius);
		slice_count_str = std::to_string(slice_count);

		// Defines.
		defines[0] = { "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() };
		defines[1] = { "NDC_TO_VIEW_MUL", ndc_to_view_mul_str.c_str() };
		defines[2] = { "NDC_TO_VIEW_ADD", ndc_to_view_add_str.c_str() };
		defines[3] = { "EFFECT_RADIUS", radius_str.c_str() };
		defines[4] = { "SLICE_COUNT", slice_count_str.c_str() };
		defines[5] = { nullptr, nullptr };
	}

	const D3D_SHADER_MACRO* get() const
	{
		return defines;
	}

private:

	std::string viewport_pixel_size_str;
	std::string ndc_to_view_mul_str;
	std::string ndc_to_view_add_str;
	std::string radius_str;
	std::string slice_count_str;
	D3D_SHADER_MACRO defines[6];
};

// We need to pass this to all SMAA shaders on resolution change.
class SMAA_rt_metrics
{
public:

	void set(float width, float height)
	{
		val_str = std::format("float4({},{},{},{})", 1.0f / width, 1.0f / height, width, height);
		defines[0] = { "SMAA_RT_METRICS", val_str.c_str() };
		defines[1] = { nullptr, nullptr };
	}

	const D3D10_SHADER_MACRO* get() const
	{
		return defines;
	}

private:

	std::string val_str;
	D3D10_SHADER_MACRO defines[2];
};

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

// Lighting/fog
constexpr uint32_t g_ps_0xCC4507C7_hash = 0xCC4507C7;
static uintptr_t g_ps_0xCC4507C7_handle;

// Water
constexpr uint32_t g_ps_0x77691A09_hash = 0x77691A09;
static uintptr_t g_ps_0x77691A09_handle;

// Drill attack effect (1)
constexpr uint32_t g_ps_0x29709A38_hash = 0x29709A38;
static uintptr_t g_ps_0x29709A38_handle;

// Drill attack effect (2)
constexpr uint32_t g_ps_0x116DAB10_hash = 0x116DAB10;
static uintptr_t g_ps_0x116DAB10_handle;

// Drill attack effect (3)
constexpr uint32_t g_ps_0x3BA38F8D_hash = 0x3BA38F8D;
static uintptr_t g_ps_0x3BA38F8D_handle;

// Drill attack effect (4)
constexpr uint32_t g_ps_0x460951FB_hash = 0x460951FB;
static uintptr_t g_ps_0x460951FB_handle;

// Bloom downscale.
constexpr uint32_t g_ps_0xE79A291C_hash = 0xE79A291C;
static uintptr_t g_ps_0xE79A291C_handle;

// Tone maping.
// The last shader before UI, unless under water.
constexpr uint32_t g_ps_0x9FA167B7_hash = 0x9FA167B7;
static uintptr_t g_ps_0x9FA167B7_handle;
static Com_ptr<ID3D10PixelShader> g_ps_0x9FA167B7;

static UINT g_swapchain_width;
static UINT g_swapchain_height;
static Com_ptr<ID3D10SamplerState> g_smp_point_clamp;
static Com_ptr<ID3D10ShaderResourceView> g_srv_depth;
static Com_ptr<ID3D10VertexShader> g_vs_fullscreen_triangle;

// Tone response curve
//

// enum TRC_
static int g_trc = TRC_SRGB;

static float g_gamma = 2.2f;

//

// XeGTAO
constexpr int XE_GTAO_DEPTH_MIP_LEVELS = 5;
static XeGTAO_defines g_xegtao_defines;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_prefilter_depth_mip0;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_prefilter_depths;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_main_pass;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_denoise_pass;
Com_ptr<ID3D10BlendState> g_blend_xegtao;
static bool g_xegtao_enable = true;
static float g_xegtao_fov_y = 47.0f; // in degrees
static float g_xegtao_radius = 0.11f;
static int g_xegtao_slice_count = 9;
static bool is_xegtao_drawn;

// SMAA
constexpr float g_smaa_clear_color[4] = {};
static SMAA_rt_metrics g_smaa_rt_metrics;
static Com_ptr<ID3D10ShaderResourceView> g_srv_smaa_area_tex;
static Com_ptr<ID3D10ShaderResourceView> g_srv_smaa_search_tex;
static Com_ptr<ID3D10VertexShader> g_vs_smaa_edge_detection;
static Com_ptr<ID3D10PixelShader> g_ps_smaa_edge_detection;
static Com_ptr<ID3D10VertexShader> g_vs_smaa_blending_weight_calculation;
static Com_ptr<ID3D10PixelShader> g_ps_smaa_blending_weight_calculation;
static Com_ptr<ID3D10VertexShader> g_vs_smaa_neighborhood_blending;
static Com_ptr<ID3D10PixelShader> g_ps_smaa_neighborhood_blending;
static Com_ptr<ID3D10DepthStencilState> g_ds_smaa_disable_depth_replace_stencil;
static Com_ptr<ID3D10DepthStencilState> g_ds_smaa_disable_depth_use_stencil;

// AMD FFX CAS
static Com_ptr<ID3D10PixelShader> g_ps_amd_ffx_cas;
static float g_amd_ffx_cas_sharpness = 0.4f;

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

// Used by XeGTAO.
static void create_sampler_point_clamp(ID3D10Device* device, ID3D10SamplerState** smp)
{
	auto sampler_desc = default_D3D10_SAMPLER_DESC();
	ensure(device->CreateSamplerState(&sampler_desc, smp), >= 0);
}

static void reset_xegtao()
{
	g_ps_xegtao_prefilter_depth_mip0.reset();
	g_ps_xegtao_prefilter_depths.reset();
	g_ps_xegtao_main_pass.reset();
	g_ps_xegtao_denoise_pass.reset();
}

static void draw_xegtao(ID3D10Device* device, ID3D10RenderTargetView*const* rtv)
{
	// Backup states.
	//

	// Primitive topology.
	D3D10_PRIMITIVE_TOPOLOGY primitive_topology_original;
	device->IAGetPrimitiveTopology(&primitive_topology_original);

	// VS.
	Com_ptr<ID3D10VertexShader> vs_original;
	device->VSGetShader(&vs_original);

	// Viewports.
	UINT nviewports;
	device->RSGetViewports(&nviewports, nullptr);
	std::vector<D3D10_VIEWPORT> viewports_original(nviewports);
	device->RSGetViewports(&nviewports, viewports_original.data());

	// Rasterizer.
	Com_ptr<ID3D10RasterizerState> rasterizer_original;
	device->RSGetState(&rasterizer_original);

	// PS.
	Com_ptr<ID3D10PixelShader> ps_original;
	device->PSGetShader(&ps_original);
	std::array<ID3D10ShaderResourceView*, 2> srvs_original;
	device->PSGetShaderResources(0, srvs_original.size(), srvs_original.data());
	Com_ptr<ID3D10SamplerState> smp_original;
	device->PSGetSamplers(0, 1, &smp_original);

	// Blend.
	Com_ptr<ID3D10BlendState> blend_original;
	FLOAT blend_factor_original[4];
	UINT sample_mask_original;
	device->OMGetBlendState(&blend_original, blend_factor_original, &sample_mask_original);

	// RTs.
	Com_ptr<ID3D10RenderTargetView> rtv_original;
	Com_ptr<ID3D10DepthStencilView> dsv_original;
	device->OMGetRenderTargets(1, &rtv_original, &dsv_original);
	device->OMSetRenderTargets(0, nullptr, nullptr);

	//

	// Set common states.
	//

	device->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create and bind Fullscreen Triangle VS.
	[[unlikely]] if (!g_vs_fullscreen_triangle) {
		create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
	}
	device->VSSetShader(g_vs_fullscreen_triangle.get());

	device->RSSetState(nullptr);

	// Create and bind point clamp sampler.
	[[unlikely]] if (!g_smp_point_clamp) {
		create_sampler_point_clamp(device, &g_smp_point_clamp);
	}
	device->PSSetSamplers(0, 1, &g_smp_point_clamp);

	device->OMSetBlendState(nullptr, nullptr, UINT_MAX);

	//

	// Common texture description.
	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = g_swapchain_width;
	tex_desc.Height = g_swapchain_height;
	tex_desc.ArraySize = 1;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;

	// PrefilterDepths passes.
	//

	// Create texture.
	tex_desc.MipLevels = XE_GTAO_DEPTH_MIP_LEVELS;
	tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
	Com_ptr<ID3D10Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, nullptr, &tex), >= 0);

	// Create RTVs and SRVs for working depth mips.
	std::array<Com_ptr<ID3D10RenderTargetView>, XE_GTAO_DEPTH_MIP_LEVELS> rtv_working_depth_mips;
	D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = tex_desc.Format;
	rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
	std::array<Com_ptr<ID3D10ShaderResourceView>, XE_GTAO_DEPTH_MIP_LEVELS> srv_working_depth_mips;
	D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = tex_desc.Format;
	srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	for (UINT i = 0; i < XE_GTAO_DEPTH_MIP_LEVELS; ++i) {
		rtv_desc.Texture2D.MipSlice = i;
		ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &rtv_working_depth_mips[i]), >= 0);
		srv_desc.Texture2D.MostDetailedMip = i;
		ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &srv_working_depth_mips[i]), >= 0);
	}

	// Create working depth SRV with all mips.
	Com_ptr<ID3D10ShaderResourceView> srv_working_depth;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_working_depth), >= 0);

	// Prefilter depths viewport, for mip0.
	D3D10_VIEWPORT viewport = {};
	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Create prefilter depths PS, for mip0.
	[[unlikely]] if (!g_ps_xegtao_prefilter_depth_mip0) {
		g_xegtao_defines.set(g_swapchain_width, g_swapchain_height, g_xegtao_fov_y, g_xegtao_radius, g_xegtao_slice_count);
		create_pixel_shader(device, &g_ps_xegtao_prefilter_depth_mip0, L"XeGTAO_impl.hlsl", "prefilter_depths_mip0_ps", g_xegtao_defines.get());
	}

	// Bindings.
	device->OMSetRenderTargets(1, &rtv_working_depth_mips[0], nullptr);
	device->RSSetViewports(1, &viewport);
	device->PSSetShader(g_ps_xegtao_prefilter_depth_mip0.get());
	device->PSSetShaderResources(0, 1, &g_srv_depth);

	// Prefilter depths, mip0.
	device->Draw(3, 0);

	// Create prefilter depths PS, for mips 1 to 4.
	[[unlikely]] if (!g_ps_xegtao_prefilter_depths) {
		create_pixel_shader(device, &g_ps_xegtao_prefilter_depths, L"XeGTAO_impl.hlsl", "prefilter_depths_ps", g_xegtao_defines.get());
	}
	device->PSSetShader(g_ps_xegtao_prefilter_depths.get());

	// Prefilter depths, mips 1 to 4.
	for (UINT i = 1; i < 5; ++i) {
		viewport.Width = std::max(1u, viewport.Width / 2u);
		viewport.Height = std::max(1u, viewport.Height / 2u);

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_working_depth_mips[i], nullptr);
		device->RSSetViewports(1, &viewport);
		device->PSSetShaderResources(0, 1, &srv_working_depth_mips[i - 1]);

		device->Draw(3, 0);
	}

	//

	// Set common viewport.
	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;
	device->RSSetViewports(1, &viewport);

	// MainPass pass.
	//

	// Create PS.
	[[unlikely]] if (!g_ps_xegtao_main_pass) {
		create_pixel_shader(device, &g_ps_xegtao_main_pass, L"XeGTAO_impl.hlsl", "main_pass_ps", g_xegtao_defines.get());
	}

	// Create RT views.
	tex_desc.MipLevels = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
	Com_ptr<ID3D10RenderTargetView> rtv_main_pass;
	ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_main_pass), >= 0);
	Com_ptr<ID3D10ShaderResourceView> srv_main_pass;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_main_pass), >= 0);

	// Bindings.
	device->OMSetRenderTargets(1, &rtv_main_pass, nullptr);
	device->PSSetShader(g_ps_xegtao_main_pass.get());
	device->PSSetShaderResources(0, 1, &srv_working_depth);

	device->Draw(3, 0);

	//

	// DenoisePass pass
	//
	// Doing only one DenoisePass pass (as last/final pass) correspond to "Denoising level: Sharp" from the XeGTAO demo.
	//

	// Create PS.
	[[unlikely]] if (!g_ps_xegtao_denoise_pass) {
		create_pixel_shader(device, &g_ps_xegtao_denoise_pass, L"XeGTAO_impl.hlsl", "denoise_pass_ps", g_xegtao_defines.get());
	}

	// Create blend.
	[[unlikely]] if (!g_blend_xegtao) {
		auto blend_desc = default_D3D10_BLEND_DESC();
		blend_desc.BlendEnable[0] = TRUE;
		blend_desc.SrcBlend = D3D10_BLEND_DEST_COLOR;
		ensure(device->CreateBlendState(&blend_desc, &g_blend_xegtao), >= 0);
	}

	// Bindings.
	////

	device->OMSetRenderTargets(1, rtv, nullptr);
	device->PSSetShader(g_ps_xegtao_denoise_pass.get());
	device->PSSetShaderResources(1, 1, &srv_main_pass);

	#if !(DEV && SHOW_AO)
	device->OMSetBlendState(g_blend_xegtao.get(), nullptr, UINT_MAX);
	#endif

	////

	device->Draw(3, 0);

	//

	// Restore states.
	device->OMSetRenderTargets(1, &rtv_original, dsv_original.get());
	device->IASetPrimitiveTopology(primitive_topology_original);
	device->VSSetShader(vs_original.get());
	device->RSSetViewports(nviewports, viewports_original.data());
	device->RSSetState(rasterizer_original.get());
	device->PSSetShader(ps_original.get());
	device->PSSetShaderResources(0, srvs_original.size(), srvs_original.data());
	device->PSSetSamplers(0, 1, &smp_original);
	device->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

	// Have to manualy release these.
	for (size_t i = 0; i < srvs_original.size(); ++i) {
		if (srvs_original[i]) {
			srvs_original[i]->Release();
		}
	}
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
	is_xegtao_drawn = false;
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

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(&ps);
	if (!ps) {
		return false;
	}

	// Lightning/fog
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0xCC4507C7_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// Water
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0x77691A09_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		#if DEV
		// Get RT resource (texture) and texture description.
		// Make sure we always have the main scene.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			log_debug("0x77691A09 RTV wasnt what we expected it to be.");
			return false;
		}
		#endif

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// // Drill attack effect (1)
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0x29709A38_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		#if DEV
		// Get RT resource (texture) and texture description.
		// Make sure we always have the main scene.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			log_debug("0x29709A38 RTV wasnt what we expected it to be.");
			return false;
		}
		#endif

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// // Drill attack effect (2)
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0x116DAB10_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		#if DEV
		// Get RT resource (texture) and texture description.
		// Make sure we always have the main scene.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			log_debug("0x116DAB10 RTV wasnt what we expected it to be.");
			return false;
		}
		#endif

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// // Drill attack effect (3)
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0x3BA38F8D_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		#if DEV
		// Get RT resource (texture) and texture description.
		// Make sure we always have the main scene.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			log_debug("0x3BA38F8D RTV wasnt what we expected it to be.");
			return false;
		}
		#endif

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// // Drill attack effect (4)
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0x460951FB_handle) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// Bloom downscale
	if (g_xegtao_enable && !is_xegtao_drawn && (uintptr_t)ps.get() == g_ps_0xE79A291C_handle) {
		// We expect SRV to be the main scene.
		Com_ptr<ID3D10ShaderResourceView> srv_original;
		device->PSGetShaderResources(0, 1, &srv_original);

		// Get the resource and create RTV.
		Com_ptr<ID3D10Resource> resource;
		srv_original->GetResource(&resource);
		Com_ptr<ID3D10RenderTargetView> rtv;
		ensure(device->CreateRenderTargetView(resource.get(), nullptr, &rtv), >= 0);

		draw_xegtao(device, &rtv);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// Tone mapping
	if ((uintptr_t)ps.get() == g_ps_0x9FA167B7_handle) {
		// We expect RTV to be a back buffer.
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

		#if DEV && SHOW_AO
		draw_xegtao(device, &rtv_original);
		return true;
		#endif

		// 0x9FA167B7 pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_0x9FA167B7) {
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_0x9FA167B7, L"0x9FA167B7_ps.hlsl");
		}

		// Create RTs and views.
		////

		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		
		// Linear.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_0x9FA167B7_linear;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_0x9FA167B7_linear), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_0x9FA167B7_linear;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_0x9FA167B7_linear), >= 0);

		// Delinearized.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_0x9FA167B7_delinearized;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_0x9FA167B7_delinearized), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_0x9FA167B7_delinearized;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_0x9FA167B7_delinearized), >= 0);

		////
		
		// Bindings.
		const std::array rtvs_0x9FA167B7 = { rtv_0x9FA167B7_linear.get(), rtv_0x9FA167B7_delinearized.get() };
		device->OMSetRenderTargets(rtvs_0x9FA167B7.size(), rtvs_0x9FA167B7.data(), nullptr);
		device->PSSetShader(g_ps_0x9FA167B7.get());

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		#if DEV
		// Primitive topology should be D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology;
		device->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D10_FILTER_MIN_MAG_MIP_POINT
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(0, 1, &smp);
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_POINT || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		device->PSGetSamplers(1, 1, smp.reset_and_get_address());
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// SMAAEdgeDetection pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_edge_detection) {
			g_smaa_rt_metrics.set(tex_desc.Width, tex_desc.Height);
			create_vertex_shader(device, &g_vs_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_edge_detection) {
			create_pixel_shader(device, &g_ps_smaa_edge_detection, L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_replace_stencil) {
			D3D10_DEPTH_STENCIL_DESC desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, &g_ds_smaa_disable_depth_replace_stencil), >= 0);
		}

		// Create DSV.
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10DepthStencilView> dsv;
		ensure(device->CreateDepthStencilView(tex.get(), nullptr, &dsv), >= 0);

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_edge_detection;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_smaa_edge_detection), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_edge_detection;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_smaa_edge_detection), >= 0);

		// Bindings.
		device->OMSetDepthStencilState(g_ds_smaa_disable_depth_replace_stencil.get(), 1);
		device->OMSetRenderTargets(1, &rtv_smaa_edge_detection, dsv.get());
		device->VSSetShader(g_vs_smaa_edge_detection.get());
		device->PSSetShader(g_ps_smaa_edge_detection.get());
		const std::array ps_srvs_smaa_edge_detection = { srv_0x9FA167B7_delinearized.get(), g_srv_depth.get() };
		device->PSSetShaderResources(0, ps_srvs_smaa_edge_detection.size(), ps_srvs_smaa_edge_detection.data());

		device->ClearRenderTargetView(rtv_smaa_edge_detection.get(), g_smaa_clear_color);
		device->ClearDepthStencilView(dsv.get(), D3D10_CLEAR_STENCIL, 1.0f, 0);
		device->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device, &g_vs_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device, &g_ps_smaa_blending_weight_calculation, L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!g_srv_smaa_area_tex) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = AREATEX_WIDTH;
			tex_desc.Height = AREATEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			D3D10_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = areaTexBytes;
			subresource_data.SysMemPitch = AREATEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.reset_and_get_address()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_smaa_area_tex), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!g_srv_smaa_search_tex) {
			D3D10_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = SEARCHTEX_WIDTH;
			tex_desc.Height = SEARCHTEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			D3D10_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = searchTexBytes;
			subresource_data.SysMemPitch = SEARCHTEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.reset_and_get_address()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_smaa_search_tex), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_use_stencil) {
			auto desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D10_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, &g_ds_smaa_disable_depth_use_stencil), >= 0);
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_blending_weight_calculation;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_smaa_blending_weight_calculation), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_blending_weight_calculation;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_smaa_blending_weight_calculation), >= 0);

		// Bindings.
		device->OMSetDepthStencilState(g_ds_smaa_disable_depth_use_stencil.get(), 1);
		device->OMSetRenderTargets(1, &rtv_smaa_blending_weight_calculation, dsv.get());
		device->VSSetShader(g_vs_smaa_blending_weight_calculation.get());
		device->PSSetShader(g_ps_smaa_blending_weight_calculation.get());
		const std::array ps_srvs_smaa_blending_weight_calculation = { srv_smaa_edge_detection.get(), g_srv_smaa_area_tex.get(), g_srv_smaa_search_tex.get() };
		device->PSSetShaderResources(0, ps_srvs_smaa_blending_weight_calculation.size(), ps_srvs_smaa_blending_weight_calculation.data());

		device->ClearRenderTargetView(rtv_smaa_blending_weight_calculation.get(), g_smaa_clear_color);
		device->Draw(3, 0);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_neighborhood_blending) {
			create_vertex_shader(device, &g_vs_smaa_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_neighborhood_blending) {
			create_pixel_shader(device, &g_ps_smaa_neighborhood_blending, L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_neighborhood_blending;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_smaa_neighborhood_blending), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_neighborhood_blending;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_smaa_neighborhood_blending), >= 0);

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_smaa_neighborhood_blending, nullptr);
		device->VSSetShader(g_vs_smaa_neighborhood_blending.get());
		device->PSSetShader(g_ps_smaa_neighborhood_blending.get());
		const std::array ps_srvs_neighborhood_blending = { srv_0x9FA167B7_linear.get(), srv_smaa_blending_weight_calculation.get() };
		device->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

		device->Draw(3, 0);

		//

		// AMD FFX CAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_amd_ffx_cas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, &g_ps_amd_ffx_cas, L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->VSSetShader(g_vs_fullscreen_triangle.get());
		device->PSSetShader(g_ps_amd_ffx_cas.get());
		device->PSSetShaderResources(0, 1, &srv_smaa_neighborhood_blending);

		device->Draw(3, 0);

		//

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
			switch (hash) {
				case g_ps_0x9FA167B7_hash:
					g_ps_0x9FA167B7_handle = pipeline.handle;
					break;
				case g_ps_0xE79A291C_hash:
					g_ps_0xE79A291C_handle = pipeline.handle;
					break;
				case g_ps_0x29709A38_hash:
					g_ps_0x29709A38_handle = pipeline.handle;
					break;
				case g_ps_0x116DAB10_hash:
					g_ps_0x116DAB10_handle = pipeline.handle;
					break;
				case g_ps_0x3BA38F8D_hash:
					g_ps_0x3BA38F8D_handle = pipeline.handle;
					break;
				case g_ps_0x460951FB_hash:
					g_ps_0x460951FB_handle = pipeline.handle;
					break;
				case g_ps_0xCC4507C7_hash:
					g_ps_0xCC4507C7_handle = pipeline.handle;
					break;
				case g_ps_0x77691A09_hash:
					g_ps_0x77691A09_handle = pipeline.handle;
					break;
			}
		}
	}
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Upgrade render targets.
	if (((desc.usage & reshade::api::resource_usage::render_target) != 0)) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
	}

	return false;
}

static void on_init_resource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state, reshade::api::resource resource)
{
	if (desc.texture.format == reshade::api::format::r24_unorm_x8_uint) {
		auto native_device = (ID3D10Device*)device->get_native();
		ensure(native_device->CreateShaderResourceView((ID3D10Resource*)resource.handle, nullptr, g_srv_depth.reset_and_get_address()), >= 0);
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	const auto resource_desc = device->get_resource_desc(resource);

	// Render targets.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
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

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// Force game into borderless window (borderless fullscreen, the main intent).
	// Windowed mode (non borderless) still works.
	// Fixes wrong window size after alt + tab.
	SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
	SetWindowPos((HWND)hwnd, HWND_TOP, 0, 0, desc.back_buffer.texture.width, desc.back_buffer.texture.height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	// TODO: Upgrade to rgb10. The game renders underwater effects after the tone mapping 0x9FA167B7,
	// it ping-pongs backbuffer (does copy!) we will have to handle this manually.
	// Upgrading resources does work, but we would also upgrade resources that we don't want to upgrade.
	//desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_state = false;
	desc.fullscreen_refresh_rate = 0.0f;
	desc.sync_interval = 0;
	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	native_swapchain->GetDesc(&desc);
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Reset resolution dependent resources.
	reset_xegtao();
	g_vs_smaa_edge_detection.reset();
	g_ps_smaa_edge_detection.reset();
	g_vs_smaa_blending_weight_calculation.reset();
	g_ps_smaa_blending_weight_calculation.reset();
	g_vs_smaa_neighborhood_blending.reset();
	g_ps_smaa_neighborhood_blending.reset();
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
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOEnable", g_xegtao_enable)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOEnable", g_xegtao_enable);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAORadius", g_xegtao_radius)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAORadius", g_xegtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	if (ImGui::Checkbox("XeGTAO enable", &g_xegtao_enable)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOEnable", g_xegtao_enable);
		reset_xegtao();
	}
	ImGui::BeginDisabled(!g_xegtao_enable);
	ImGui::InputFloat("XeGTAO FOV Y", &g_xegtao_fov_y);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_xegtao_fov_y = std::clamp(g_xegtao_fov_y, 0.0f, 360.0f);
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
		reset_xegtao();
	}
	if (ImGui::SliderFloat("XeGTAO radius", &g_xegtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAORadius", g_xegtao_radius);
		reset_xegtao();
	}
	if (ImGui::SliderInt("XeGTAO slice count", &g_xegtao_slice_count, 0, 45, "%d", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count);
		reset_xegtao();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();
	static constexpr std::array g_trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, g_trc_items.data(), g_trc_items.size())) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
		g_ps_0x9FA167B7.reset();
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
		g_ps_0x9FA167B7.reset();
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::Spacing();
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 10.0f, 1000.0f);
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "Bioshock2GrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Bioshock2GrapicalUpgrade v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/Bioshock2GraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			reshade::register_event<reshade::addon_event::present>(on_present);
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
