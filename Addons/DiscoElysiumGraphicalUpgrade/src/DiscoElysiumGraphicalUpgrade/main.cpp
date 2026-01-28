#include "Common.h"
#include "Helpers.h"
#include "AreaTex.h"
#include "SearchTex.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0

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

	const D3D_SHADER_MACRO* get() const
	{
		return defines;
	}

private:

	std::string val_str;
	D3D_SHADER_MACRO defines[2] = {};
};

// FO/Post/Vignette
constexpr uint32_t g_ps_vigenette_0x49E25D6C_hash = 0x4C02E28F;
static uintptr_t g_ps_vigenette_0x49E25D6C;

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

static Com_ptr<ID3D11SamplerState> g_smp_point_clamp;
static Com_ptr<ID3D11ComputeShader> g_cs_linearize;

// SMAA
constexpr float g_smaa_clear_color[4] = {};
static SMAA_rt_metrics g_smaa_rt_metrics;
static Com_ptr<ID3D11ShaderResourceView> g_srv_smaa_area_tex;
static Com_ptr<ID3D11ShaderResourceView> g_srv_smaa_search_tex;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_edge_detection;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_edge_detection;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_blending_weight_calculation;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_blending_weight_calculation;
static Com_ptr<ID3D11VertexShader> g_vs_smaa_neighborhood_blending;
static Com_ptr<ID3D11PixelShader> g_ps_smaa_neighborhood_blending;
static Com_ptr<ID3D11DepthStencilState> g_ds_smaa_disable_depth_replace_stencil;
static Com_ptr<ID3D11DepthStencilState> g_ds_smaa_disable_depth_use_stencil;
static bool g_smaa_enable = true;

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

static void create_sampler_point_clamp(ID3D11Device* device, ID3D11SamplerState** smp)
{
	CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}

static void on_present(reshade::api::command_queue* queue, reshade::api::swapchain* swapchain, const reshade::api::rect* source_rect, const reshade::api::rect* dest_rect, uint32_t dirty_rect_count, const reshade::api::rect* dirty_rects)
{
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
	
	// We are using PS as hash for draw calls.
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(ps.put(), nullptr, nullptr);
	if (!ps) {
		return false;
	}

	// FIXME: Rendering order in the game is a mess so some UI elemets are rendered before vigenette.
	// Examples: Some film strips but not all, popup text in the games world.
	// We should render SMAA before most of the UI.
	// For an example green outline (selection indicator) should be affected by SMAA.
	if (g_smaa_enable && (uintptr_t)ps.get() == g_ps_vigenette_0x49E25D6C) {

		// Get device.
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(device.put());

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(0, 1, smp.put());
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup PS.
		Com_ptr<ID3D11PixelShader> ps_original;
		ctx->PSGetShader(ps_original.put(), nullptr, nullptr);
		Com_ptr<ID3D11SamplerState> ps_smp_original;
		ctx->PSGetSamplers(1, 1, ps_smp_original.put());
		Com_ptr<ID3D11ShaderResourceView> ps_srv_original = {};
		ctx->PSGetShaderResources(1, 1, ps_srv_original.put());

		// Backup DepthStencil
		Com_ptr<ID3D11DepthStencilState> ds_original;
		UINT stencil_ref_original;
		ctx->OMGetDepthStencilState(ds_original.put(), &stencil_ref_original);

		// Backup RT.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		Com_ptr<ID3D11DepthStencilView> dsv_original;
		ctx->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());

		// SRV0 should be the scene in sRGB color space with some UI elements.
		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		ctx->PSGetShaderResources(0, 1, srv_scene.put());

		// Get scene resource and texture description.
		Com_ptr<ID3D11Resource> resource;
		srv_scene->GetResource(resource.put());
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Create RTV scene.
		Com_ptr<ID3D11RenderTargetView> rtv_scene;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_scene.put()), >= 0);

		// LinearizeScene pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs_linearize) {
			create_compute_shader(device.get(), g_cs_linearize.put(), L"Linearize.hlsl");
		}

		// Create RT and views.
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11UnorderedAccessView> uav_linearized_scene;
		ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, uav_linearized_scene.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_linearized_scene;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_linearized_scene.put()), >= 0);

		// Bindings.
		ctx->CSSetShader(g_cs_linearize.get(), nullptr, 0);
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_linearized_scene, nullptr);
		ctx->CSSetShaderResources(0, 1, &srv_scene);

		ctx->Dispatch((tex_desc.Width + 8 - 1) / 8, (tex_desc.Height + 8 - 1) / 8, 1);

		constexpr ID3D11UnorderedAccessView* uav_null = nullptr;
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_null, nullptr);

		//
		
		// SMAAEdgeDetection pass
		//

		[[unlikely]] if (!g_smp_point_clamp) {
			create_sampler_point_clamp(device.get(), g_smp_point_clamp.put());
		}

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_edge_detection) {
			g_smaa_rt_metrics.set(tex_desc.Width, tex_desc.Height);
			create_vertex_shader(device.get(), g_vs_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_edge_detection) {
			create_pixel_shader(device.get(), g_ps_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_replace_stencil) {
			CD3D11_DEPTH_STENCIL_DESC ds_desc(D3D11_DEFAULT);
			ds_desc.DepthEnable = FALSE;
			ds_desc.StencilEnable = TRUE;
			ds_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&ds_desc, g_ds_smaa_disable_depth_replace_stencil.put()), >= 0);
		}

		// Create DSV.
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11DepthStencilView> dsv;
		ensure(device->CreateDepthStencilView(tex.get(), nullptr, dsv.put()), >= 0);

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_edge_detection;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_edge_detection.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_edge_detection;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_edge_detection.put()), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds_smaa_disable_depth_replace_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_edge_detection, dsv.get());
		ctx->VSSetShader(g_vs_smaa_edge_detection.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_edge_detection.get(), nullptr, 0);
		ctx->PSSetSamplers(1, 1, &g_smp_point_clamp);
		ctx->PSSetShaderResources(0, 1, &srv_scene);

		ctx->ClearRenderTargetView(rtv_smaa_edge_detection.get(), g_smaa_clear_color);
		ctx->ClearDepthStencilView(dsv.get(), D3D11_CLEAR_STENCIL, 1.0f, 0);
		ctx->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device.get(), g_vs_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device.get(), g_ps_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
		}

		// Create area texture.
		[[unlikely]] if (!g_srv_smaa_area_tex) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = AREATEX_WIDTH;
			tex_desc.Height = AREATEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = areaTexBytes;
			subresource_data.SysMemPitch = AREATEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv_smaa_area_tex.put()), >= 0);
		}

		// Create search texture.
		[[unlikely]] if (!g_srv_smaa_search_tex) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = SEARCHTEX_WIDTH;
			tex_desc.Height = SEARCHTEX_HEIGHT;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA subresource_data = {};
			subresource_data.pSysMem = searchTexBytes;
			subresource_data.SysMemPitch = SEARCHTEX_PITCH;
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv_smaa_search_tex.put()), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_use_stencil) {
			CD3D11_DEPTH_STENCIL_DESC ds_desc(D3D11_DEFAULT);
			ds_desc.DepthEnable = FALSE;
			ds_desc.StencilEnable = TRUE;
			ds_desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&ds_desc, g_ds_smaa_disable_depth_use_stencil.put()), >= 0);
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_smaa_blending_weight_calculation;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_blending_weight_calculation.put()), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_smaa_blending_weight_calculation;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_blending_weight_calculation.put()), >= 0);

		// Bindings.
		ctx->OMSetDepthStencilState(g_ds_smaa_disable_depth_use_stencil.get(), 1);
		ctx->OMSetRenderTargets(1, &rtv_smaa_blending_weight_calculation, dsv.get());
		ctx->VSSetShader(g_vs_smaa_blending_weight_calculation.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_blending_weight_calculation.get(), nullptr, 0);
		const std::array ps_srvs_smaa_blending_weight_calculation = { srv_smaa_edge_detection.get(), g_srv_smaa_area_tex.get(), g_srv_smaa_search_tex.get() };
		ctx->PSSetShaderResources(0, ps_srvs_smaa_blending_weight_calculation.size(), ps_srvs_smaa_blending_weight_calculation.data());

		ctx->ClearRenderTargetView(rtv_smaa_blending_weight_calculation.get(), g_smaa_clear_color);
		ctx->Draw(3, 0);

		//

		// SMAANeighborhoodBlending pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_neighborhood_blending) {
			create_vertex_shader(device.get(), g_vs_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_neighborhood_blending) {
			create_pixel_shader(device.get(), g_ps_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_scene, nullptr);
		ctx->VSSetShader(g_vs_smaa_neighborhood_blending.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_smaa_neighborhood_blending.get(), nullptr, 0);
		const std::array ps_srvs_neighborhood_blending = { srv_linearized_scene.get(), srv_smaa_blending_weight_calculation.get() };
		ctx->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

		ctx->Draw(3, 0);

		//

		// Restore.
		ctx->OMSetDepthStencilState(ds_original.get(), stencil_ref_original);
		ctx->OMSetRenderTargets(1, &rtv_original, dsv_original.get());
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->PSSetShader(ps_original.get(), nullptr, 0);
		ctx->PSSetSamplers(1, 1, &ps_smp_original);
		const std::array ps_srvs_restore = { srv_scene.get(), ps_srv_original.get() };
		ctx->PSSetShaderResources(0, ps_srvs_restore.size(), ps_srvs_restore.data());

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	return false;
}

static bool on_resolve_texture_region(reshade::api::command_list* cmd_list, reshade::api::resource source, uint32_t source_subresource, const reshade::api::subresource_box* source_box, reshade::api::resource dest, uint32_t dest_subresource, uint32_t dest_x, uint32_t dest_y, uint32_t dest_z, reshade::api::format format)
{
	// If we let the game do the resolve it will use a wrong format after we made RT format upgrades.
	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	ctx->ResolveSubresource((ID3D11Resource*)dest.handle, dest_subresource, (ID3D11Resource*)source.handle, source_subresource, DXGI_FORMAT_R16G16B16A16_FLOAT);
	return true;
}

static void on_init_pipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint32_t subobject_count, const reshade::api::pipeline_subobject* subobjects, reshade::api::pipeline pipeline)
{
	for (uint32_t i = 0; i < subobject_count; ++i) {
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			if (hash == g_ps_vigenette_0x49E25D6C_hash) {
				g_ps_vigenette_0x49E25D6C = pipeline.handle;
			}
		}
	}
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and filter out the one specific RT (64x32) that expects rgba8. 
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 && desc.texture.width != 64 && desc.texture.height != 32) {
		if (desc.texture.format == reshade::api::format::r8g8b8a8_typeless) {
			desc.texture.format = reshade::api::format::r16g16b16a16_typeless;
			return true;
		}
	}

	// Can't just filter RTs for this one,
	// cause resolve destination for MSAA may not have RT bind flag.
	if (!initial_data && desc.texture.format == reshade::api::format::r11g11b10_float) {	
		desc.texture.format = reshade::api::format::r16g16b16a16_float;
		return true;
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

		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_typeless) {
			if (desc.format == reshade::api::format::r8g8b8a8_unorm) { // The original view format, we wanna know the type.
				desc.format = reshade::api::format::r16g16b16a16_unorm;
				return true;
			}
		}
	}

	// Can't just filter RTs for this one,
	// cause resolve destination for MSAA may not have RT bind flag.
	if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
		desc.format = reshade::api::format::r16g16b16a16_float;
		return true;
	}

	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.sync_interval = 0;

	// Reset resolution dependent resources.
	g_vs_smaa_edge_detection.reset();
	g_ps_smaa_edge_detection.reset();
	g_vs_smaa_blending_weight_calculation.reset();
	g_ps_smaa_blending_weight_calculation.reset();
	g_vs_smaa_neighborhood_blending.reset();
	g_ps_smaa_neighborhood_blending.reset();

	return true;
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
	g_smp_point_clamp.reset();
	g_cs_linearize.reset();

	// SMAA
	g_srv_smaa_area_tex.reset();
	g_srv_smaa_search_tex.reset();
	g_vs_smaa_edge_detection.reset();
	g_ps_smaa_edge_detection.reset();
	g_vs_smaa_blending_weight_calculation.reset();
	g_ps_smaa_blending_weight_calculation.reset();
	g_vs_smaa_neighborhood_blending.reset();
	g_ps_smaa_neighborhood_blending.reset();
	g_ds_smaa_disable_depth_replace_stencil.reset();
	g_ds_smaa_disable_depth_use_stencil.reset();
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "SMAAEnable", g_smaa_enable)) {
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "SMAAEnable", g_smaa_enable);
	}
	if (!reshade::get_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit)) {
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
	}
	g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	if (!reshade::get_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "AccountedError", g_user_set_accounted_error)) {
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
	}
	g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	if(ImGui::Checkbox("Enable SMAA", &g_smaa_enable)) {
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "SMAAEnable", g_smaa_enable);
	}
	ImGui::InputFloat("FPS limit", &g_user_set_fps_limit);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_fps_limit = std::clamp(g_user_set_fps_limit, 1.0f, 1000.0f);
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "FPSLimit", g_user_set_fps_limit);
		g_frame_interval = std::chrono::duration<double>(1.0 / (double)g_user_set_fps_limit);
	}
	ImGui::InputInt("Accounted thread sleep error in ms", &g_user_set_accounted_error, 0, 0);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_user_set_accounted_error = std::clamp(g_user_set_accounted_error, 0, 1000);
		reshade::set_config_value(nullptr, "DiscoElysiumGraphicalUpgrade", "AccountedError", g_user_set_accounted_error);
		g_accounted_error = std::chrono::duration<double>((double)g_user_set_accounted_error / 1000.0);
	}
}

extern "C" __declspec(dllexport) const char* NAME = "DiscoElysiumGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "DiscoElysiumGraphicalUpgrade v2.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/DiscoElysiumGraphicalUpgrade";

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
			reshade::register_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
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
