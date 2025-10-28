#include "Common.h"
#include "Helpers.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0

// We need to pass this to all XeGTAO CSes on resolution change or XeGTAO user settings change.
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

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

// HBAO - main pass
constexpr uint32_t g_ps_0x32C4EB43_hash = 0x32C4EB43;
static uintptr_t g_ps_0x32C4EB43;

// HBAO - pack 1
constexpr uint32_t g_ps_0xF68A9768_hash = 0xF68A9768;
static uintptr_t g_ps_0xF68A9768;

// HBAO - denoise pass 1
constexpr uint32_t g_ps_0xEE6C036E_hash = 0xEE6C036E;
static uintptr_t g_ps_0xEE6C036E;

// HBAO - pack 2
constexpr uint32_t g_ps_0x7666BE29_hash = 0x7666BE29;
static uintptr_t g_ps_0x7666BE29 ;

// HBAO - denoise pass 2 and apply (the last HBAO pass).
constexpr uint32_t g_ps_0xF4EC5E4C_hash = 0xF4EC5E4C;
static uintptr_t g_ps_0xF4EC5E4C;

// The last post process before UI.
constexpr uint32_t g_ps_0x2AC9C1EF_hash = 0x2AC9C1EF;
static uintptr_t g_ps_0x2AC9C1EF;
static Com_ptr<ID3D11PixelShader> g_ps_0x2AC9C1EF_custom;

static Com_ptr<ID3D11SamplerState> g_smp_point_clamp;

// XeGTAO
constexpr size_t XE_GTAO_DEPTH_MIP_LEVELS = 5;
constexpr UINT XE_GTAO_NUMTHREADS_X = 8;
constexpr UINT XE_GTAO_NUMTHREADS_Y = 8;
static XeGTAO_defines g_xegtao_defines;
static Com_ptr<ID3D11ComputeShader> g_cs_xegtao_prefilter_depths16x16;
static Com_ptr<ID3D11ComputeShader> g_cs_xegtao_main_pass;
static Com_ptr<ID3D11ComputeShader> g_cs_xegtao_denoise_pass;
static Com_ptr<ID3D11PixelShader> g_ps_load_ao;
static float g_xegtao_fov_y = 55.0f; // in degrees
static float g_xegtao_radius = 0.5;
static int g_xegtao_slice_count = 9;

static Com_ptr<ID3D11VertexShader> g_vs_fullscreen_triangle;
static Com_ptr<ID3D11PixelShader> g_ps_linearize;

// AMD FFX CAS
static Com_ptr<ID3D11PixelShader> g_ps_amd_ffx_cas;
static float g_amd_ffx_cas_sharpness = 0.4f;

static bool g_disable_lens_flare;

#if DEV && SHOW_AO
static Com_ptr<ID3D11ShaderResourceView> g_srv_ao;
static Com_ptr<ID3D11PixelShader> g_ps_sample;
#endif

static std::filesystem::path get_shaders_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, MAX_PATH);
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

static void create_vertex_shader(ID3D11Device* device, ID3D11VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "vs_5_0", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vs), >= 0);
}

static void create_pixel_shader(ID3D11Device* device, ID3D11PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "ps_5_0", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, ps), >= 0);
}

static void create_compute_shader(ID3D11Device* device, ID3D11ComputeShader** cs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(&code, file, "cs_5_0", entry_point, defines);
	ensure(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, cs), >= 0);
}

// Used by XeGTAO.
static void create_sampler_point_clamp(ID3D11Device* device, ID3D11SamplerState** smp)
{
	D3D11_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	ensure(device->CreateSamplerState(&sampler_desc, smp), >= 0);
}

static void reset_xegtao()
{
	// We only need to reset CSes.
	g_cs_xegtao_prefilter_depths16x16.reset();
	g_cs_xegtao_main_pass.reset();
	g_cs_xegtao_denoise_pass.reset();
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	
	// We will use PS as a hash.
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(&ps, nullptr, nullptr);
	const auto hash = (uintptr_t)ps.get();

	if (hash == g_ps_0x32C4EB43 || hash == g_ps_0xF68A9768 || hash == g_ps_0xEE6C036E || hash == g_ps_0x7666BE29) {
		return true;
	}
	if (hash == g_ps_0xF4EC5E4C) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(&device);
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// Maybe we should skip all this and just make some assumptions?
		Com_ptr<ID3D11Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Set common texture description.
		tex_desc.SampleDesc.Count = 1;
		tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

		// XeGTAOPrefilterDepths16x16 pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs_xegtao_prefilter_depths16x16) {
			g_xegtao_defines.set(tex_desc.Width, tex_desc.Height, g_xegtao_fov_y, g_xegtao_radius, g_xegtao_slice_count);
			create_compute_shader(device.get(), &g_cs_xegtao_prefilter_depths16x16, L"XeGTAO_impl.hlsl", "prefilter_depths16x16_cs", g_xegtao_defines.get());
		}

		// Already linearized depth should be bound to slot 1.
		Com_ptr<ID3D11ShaderResourceView> srv_linearized_depth;
		ctx->PSGetShaderResources(1, 1, &srv_linearized_depth);

		// Create sampler.
		[[unlikely]] if (!g_smp_point_clamp) {
			create_sampler_point_clamp(device.get(), &g_smp_point_clamp);
		}

		// Create prefilter depths views.
		tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
		tex_desc.MipLevels = XE_GTAO_DEPTH_MIP_LEVELS;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		std::array<ID3D11UnorderedAccessView*, XE_GTAO_DEPTH_MIP_LEVELS> uav_prefilter_depths;
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = tex_desc.Format;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		for (int i = 0; i < uav_prefilter_depths.size(); ++i) {
			uav_desc.Texture2D.MipSlice = i;
			ensure(device->CreateUnorderedAccessView(tex.get(), &uav_desc, &uav_prefilter_depths[i]), >= 0);
		}
		Com_ptr<ID3D11ShaderResourceView> srv_prefilter_depths;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_prefilter_depths), >= 0);

		// Bindings.
		ctx->CSSetShader(g_cs_xegtao_prefilter_depths16x16.get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &srv_linearized_depth);
		ctx->CSSetSamplers(0, 1, &g_smp_point_clamp);
		ctx->CSSetUnorderedAccessViews(2, uav_prefilter_depths.size(), uav_prefilter_depths.data(), nullptr);

		ctx->Dispatch((tex_desc.Width + 16 - 1) / 16, (tex_desc.Height + 16 - 1) / 16, 1);

		// Unbind UAVs and release uav_prefilter_depths.
		{
			static constexpr std::array<ID3D11UnorderedAccessView*, uav_prefilter_depths.size()> uav_nulls = {};
			ctx->CSSetUnorderedAccessViews(2, uav_nulls.size(), uav_nulls.data(), nullptr);
			for (int i = 0; i < uav_prefilter_depths.size(); ++i) {
				uav_prefilter_depths[i]->Release();
			}
		}

		//

		// Set common texture description.
		tex_desc.Format = DXGI_FORMAT_R8_UNORM;
		tex_desc.MipLevels = 1;

		// XeGTAOMainPass pass
		//

		// Create CS.
		[[unlikely]] if (!g_cs_xegtao_main_pass) {
			create_compute_shader(device.get(), &g_cs_xegtao_main_pass, L"XeGTAO_impl.hlsl", "main_pass_cs", g_xegtao_defines.get());
		}

		// Create AO term views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11UnorderedAccessView> uav_xegtao_aoterm;
		ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, &uav_xegtao_aoterm), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_xegtao_aoterm;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_xegtao_aoterm), >= 0);

		// Create edges views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11UnorderedAccessView> uav_xegtao_edges;
		ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, &uav_xegtao_edges), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_xegtao_edges;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_xegtao_edges), >= 0);

		// Bindings.
		ctx->CSSetShader(g_cs_xegtao_main_pass.get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &srv_prefilter_depths);
		const std::array uavs_xegtao_main_pass = { uav_xegtao_aoterm.get(), uav_xegtao_edges.get() };
		ctx->CSSetUnorderedAccessViews(0, uavs_xegtao_main_pass.size(), uavs_xegtao_main_pass.data(), nullptr);

		ctx->Dispatch((tex_desc.Width + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, (tex_desc.Height + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, 1);

		// Unbind UAVs.
		{
			static constexpr std::array<ID3D11UnorderedAccessView*, 2> uav_nulls = {};
			ctx->CSSetUnorderedAccessViews(0, uav_nulls.size(), uav_nulls.data(), nullptr);
		}

		//

		// XeGTAODenoisePass pass
		//
		// Doing only one XeGTAODenoisePass pass (as last/final pass) correspond to "Denoising level: Sharp" from the XeGTAO demo.
		//

		// Create CS.
		[[unlikely]] if (!g_cs_xegtao_denoise_pass) {
			create_compute_shader(device.get(), &g_cs_xegtao_denoise_pass, L"XeGTAO_impl.hlsl", "denoise_pass_cs", g_xegtao_defines.get());
		}

		// Create final AO term views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11UnorderedAccessView> uav_xegtao_final_aoterm;
		ensure(device->CreateUnorderedAccessView(tex.get(), nullptr, &uav_xegtao_final_aoterm), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_xegtao_final_aoterm;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_xegtao_final_aoterm), >= 0);

		// Bindings.
		ctx->CSSetShader(g_cs_xegtao_denoise_pass.get(), nullptr, 0);
		const std::array srvs_xegtao_denoise = { srv_xegtao_aoterm.get(), srv_xegtao_edges.get() };
		ctx->CSSetShaderResources(0, srvs_xegtao_denoise.size(), srvs_xegtao_denoise.data());
		ctx->CSSetUnorderedAccessViews(0, 1, &uav_xegtao_final_aoterm, nullptr);

		ctx->Dispatch((tex_desc.Width + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (tex_desc.Height + XE_GTAO_NUMTHREADS_X - 1) / XE_GTAO_NUMTHREADS_X, 1);

		// Unbind UAVs.
		{
			static constexpr std::array<ID3D11UnorderedAccessView*, 1> uav_nulls = {};
			ctx->CSSetUnorderedAccessViews(0, uav_nulls.size(), uav_nulls.data(), nullptr);
		}

		//

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("0xF4EC5E4C: The expected primitive topology wasn't what we expected it to be!");
		}
		#endif

		// XeGTAOLoadAO pass
		//
		// We rely on the existing blend to apply the AO.
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_load_ao) {
			create_pixel_shader(device.get(), &g_ps_load_ao, L"XeGTAO_impl.hlsl", "load_ao_ps");
		}

		// Bindings.
		ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_load_ao.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_xegtao_final_aoterm);

		#if DEV && SHOW_AO
		// Disable blending so we can see the raw AO.
		ctx->OMSetBlendState(nullptr, nullptr, UINT_MAX);
		#endif

		ctx->Draw(3, 0);

		//

		#if DEV && SHOW_AO
		ensure(resource->QueryInterface(tex.reset_and_get_address()), >= 0);
		tex->GetDesc(&tex_desc);

		// Copy RT and create SRV of the copy.
		Com_ptr<ID3D11Texture2D> tex_copy;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, &tex_copy), >= 0);
		ctx->CopyResource(tex_copy.get(), tex.get());
		ensure(device->CreateShaderResourceView(tex_copy.get(), nullptr, g_srv_ao.reset_and_get_address()), >= 0);
		#endif

		return true;
	}
	if (hash == g_ps_0x2AC9C1EF) {
		Com_ptr<ID3D11Device> device;
		ctx->GetDevice(&device);

		#if DEV && SHOW_AO
		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_sample) {
			create_pixel_shader(device.get(), &g_ps_sample, L"Sample_ps.hlsl");
		}

		// Bindings.
		ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, g_srv_ao.get_address());
		ctx->PSSetShader(g_ps_sample.get(), nullptr, 0);

		ctx->Draw(3, 0);
		return true;
		#endif

		Com_ptr<ID3D11RenderTargetView> rtv_original;
		ctx->OMGetRenderTargets(1, &rtv_original, nullptr);

		// Get RT resource (texture) and texture description.
		// Maybe we should skip all this and just make some assumptions?
		Com_ptr<ID3D11Resource> resource;
		rtv_original->GetResource(&resource);
		Com_ptr<ID3D11Texture2D> tex;
		ensure(resource->QueryInterface(&tex), >= 0);
		D3D11_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		// Set common texture description.
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// 0x2AC9C1EF pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_0x2AC9C1EF_custom) {
			D3D_SHADER_MACRO defines[2] = {};
			if (g_disable_lens_flare) {
				defines[0] = { "DISABLE_LENS_FLARE", nullptr };
			}
			create_pixel_shader(device.get(), &g_ps_0x2AC9C1EF_custom, L"0x2AC9C1EF_ps.hlsl", "main", defines);
		}

		// Create 0x2AC9C1EF views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_0x2AC9C1EF;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_0x2AC9C1EF), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_0x2AC9C1EF;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_0x2AC9C1EF), >= 0);

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_0x2AC9C1EF, nullptr);
		ctx->PSSetShader(g_ps_0x2AC9C1EF_custom.get(), nullptr, 0);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		#if DEV
		// Primitive topology should be D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology;
		ctx->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("0x2AC9C1EF: The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D11_FILTER_MIN_MAG_LINEAR_MIP_LINEAR
		// D3D11_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD -FLT_MAX, MaxLOD FLT_MAX
		// D3D11_COMPARISON_NEVER
		Com_ptr<ID3D11SamplerState> smp;
		ctx->PSGetSamplers(1, 1, &smp);
		D3D11_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT || smp_desc.AddressU != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D11_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != -D3D11_FLOAT32_MAX || smp_desc.MaxLOD != D3D11_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D11_COMPARISON_NEVER) {
			log_debug("0x2AC9C1EF: The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Linearize pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device.get(), &g_vs_fullscreen_triangle, L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_linearize) {
			create_pixel_shader(device.get(), &g_ps_linearize, L"Linearize_ps.hlsl");
		}

		// Create Linearize views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.reset_and_get_address()), >= 0);
		Com_ptr<ID3D11RenderTargetView> rtv_linearize;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_linearize), >= 0);
		Com_ptr<ID3D11ShaderResourceView> srv_linearize;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_linearize), >= 0);

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_linearize, nullptr);
		ctx->VSSetShader(g_vs_fullscreen_triangle.get(), nullptr, 0);
		ctx->PSSetShader(g_ps_linearize.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_0x2AC9C1EF);

		ctx->Draw(3, 0);

		//

		// AMD FFX CAS pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_amd_ffx_cas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device.get(), &g_ps_amd_ffx_cas, L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, nullptr);
		ctx->PSSetShader(g_ps_amd_ffx_cas.get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srv_linearize);

		ctx->Draw(3, 0);

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
				case g_ps_0x32C4EB43_hash:
					g_ps_0x32C4EB43 = pipeline.handle;
					break;
				case g_ps_0xF68A9768_hash:
					g_ps_0xF68A9768 = pipeline.handle;
					break;
				case g_ps_0xEE6C036E_hash:
					g_ps_0xEE6C036E = pipeline.handle;
					break;
				case g_ps_0x7666BE29_hash:
					g_ps_0x7666BE29 = pipeline.handle;
					break;
				case g_ps_0xF4EC5E4C_hash:
					g_ps_0xF4EC5E4C = pipeline.handle;
					break;
				case g_ps_0x2AC9C1EF_hash:
					g_ps_0x2AC9C1EF = pipeline.handle;
			}
		}
	}
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	// We have to manualy set proper back buffer view format in this game.
	const auto resource_desc = device->get_resource_desc(resource);
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r10g10b10a2_unorm) {
			desc.format = reshade::api::format::r10g10b10a2_unorm;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	// It looks like the game is ignoring the in game anistropic filtering option
	// and it's setting it to 4 always.
	desc.max_anisotropy = 16.0f;

	return true;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.sync_interval = 0;
	desc.fullscreen_state = false;

	// The game renders much more than UI to back buffer,
	// does it expects 8bit alpha?
	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;

	// Reset resolution dependent resources.
	reset_xegtao();

	return true;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (fullscreen) {
		// This seems to be the best place to force the game into borderless window.
		auto hwnd = (HWND)swapchain->get_hwnd();
		auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
		DXGI_SWAP_CHAIN_DESC desc;
		native_swapchain->GetDesc(&desc);
		SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, desc.BufferDesc.Width, desc.BufferDesc.Height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		return true;
	}
	return false;
}

static void on_init_device(reshade::api::device* device)
{
	// Set maximum frame latency to 1, the game is not setting this already to 1.
	// It reduces input latecy massivly if GPU bound.
	auto native_device = (IUnknown*)device->get_native();
	Com_ptr<IDXGIDevice1> device1;
	auto hr = native_device->QueryInterface(IID_PPV_ARGS(&device1));
	if (SUCCEEDED(hr)) {
		ensure(device1->SetMaximumFrameLatency(1), >= 0);
	}
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xegtao_radius)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xegtao_radius);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
	}
	if (!reshade::get_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	ImGui::InputFloat("XeGTAO FOV Y", &g_xegtao_fov_y);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		g_xegtao_fov_y = std::clamp(g_xegtao_fov_y, 0.0f, 360.0f);
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOFOVY", g_xegtao_fov_y);
		reset_xegtao();
	}
	if (ImGui::SliderFloat("XeGTAO radius", &g_xegtao_radius, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAORadius", g_xegtao_radius);
		reset_xegtao();
	}
	if (ImGui::SliderInt("XeGTAO slice count", &g_xegtao_slice_count, 0, 45, "%d", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "XeGTAOSliceCount", g_xegtao_slice_count);
		reset_xegtao();
	}
	ImGui::Spacing();
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::Spacing();
	if (ImGui::Checkbox("Disable lens flare", &g_disable_lens_flare)) {
		reshade::set_config_value(nullptr, "BFBC2GraphicalUpgrade", "DisableLensFlare", g_disable_lens_flare);
		g_ps_0x2AC9C1EF_custom.reset();
	}
}

extern "C" __declspec(dllexport) const char* NAME = "BFBC2GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "BFBC2GraphicalUpgrade v1.1.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BFBC2GraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
