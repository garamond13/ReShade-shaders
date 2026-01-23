#include "Common.h"
#include "Helpers.h"
#include "AreaTex.h"
#include "SearchTex.h"
#include "BlueNoiseTex.h"

#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0

// Tone responce curve.
enum TRC_
{
	TRC_SRGB,
	TRC_GAMMA
};

struct alignas(16) CB_graphical_upgrade_data
{
	float2 src_size;
	float2 inv_src_size;
	float2 axis;
	float sigma;
	float tex_noise_index;
};

// We need to pass this to XeGTAO PSes on resolution change or XeGTAO user settings change.
class XeGTAO_defines
{
public:

	void set(float width, float height, float fov_y, float radius, int quality)
	{
		// Values to strings.
		const float tan_half_fov_y = std::tan(fov_y * std::numbers::pi_v<float> / 180.0f * 0.5f); // radians = degrees * pi / 180.
		const float tan_half_fov_x = tan_half_fov_y * width / height;
		viewport_pixel_size_str = std::format("float2({},{})", 1.0f / width, 1.0f / height);
		ndc_to_view_mul_str = std::format("float2({},{})", tan_half_fov_x * 2.0f, tan_half_fov_y * -2.0f);
		ndc_to_view_add_str = std::format("float2({},{})", -tan_half_fov_x, tan_half_fov_y);
		radius_str = std::to_string(radius);
		slice_count_str = std::to_string(get_slice_count(quality));

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

	float get_slice_count(int quality) const
	{
		switch (quality) {
			case 0:
				return 4.0f;
			case 1:
				return 7.0f;
			case 2:
				return 10.0f;
			case 3:
				return 13.0f;
			case 4:
				return 16.0f;
		}
	}

	std::string viewport_pixel_size_str;
	std::string ndc_to_view_mul_str;
	std::string ndc_to_view_add_str;
	std::string radius_str;
	std::string slice_count_str;
	D3D_SHADER_MACRO defines[6] = {};
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
	D3D10_SHADER_MACRO defines[2] = {};
};

// Path to the GraphicalUpgrade folder.
static std::filesystem::path g_graphical_upgrade_path;

static UINT g_swapchain_width;
static UINT g_swapchain_height;

// Constant buffer.
static CB_graphical_upgrade_data g_cb_garphical_upgrade_data;
static Com_ptr<ID3D10Buffer> g_cb_graphical_upgrade;

static Com_ptr<ID3D10VertexShader> g_vs_fullscreen_triangle;

// Fog (1).
constexpr uint32_t g_ps_0xB69D6558_hash = 0xB69D6558;
constexpr GUID g_ps_0xB69D6558_guid = { 0x6920fbb, 0xcfa4, 0x48b8, { 0x94, 0xf9, 0x80, 0x7, 0x83, 0xe2, 0xbc, 0xfb } };

// Fog (2).
constexpr uint32_t g_ps_0xE2683E33_hash = 0xE2683E33;
constexpr GUID g_ps_0xE2683E33_guid = { 0x7bc430f7, 0x5db5, 0x4b25, { 0x83, 0x90, 0xc8, 0x9c, 0x74, 0xc5, 0xa7, 0xf } };

// Fog (3).
constexpr uint32_t g_ps_0xC907CF9A_hash = 0xC907CF9A;
constexpr GUID g_ps_0xC907CF9A_guid = { 0x4875f184, 0xadee, 0x4f40, { 0xb6, 0xf5, 0xfa, 0xf8, 0x54, 0xcd, 0xb4, 0x15 } };

// Fog (4).
constexpr uint32_t g_ps_0x9CFB96DA_hash = 0x9CFB96DA;
constexpr GUID g_ps_0x9CFB96DA_guid = { 0xfd87086, 0xb29a, 0x4c23, { 0xa9, 0x30, 0x8e, 0x9a, 0x98, 0x59, 0x15, 0x88 } };

// Fog (5).
constexpr uint32_t g_ps_0x3B177042_hash = 0x3B177042;
constexpr GUID g_ps_0x3B177042_guid = { 0x3897d2ee, 0xb12, 0x4dfc, { 0xa8, 0xa0, 0xae, 0x50, 0x1e, 0x17, 0x3c, 0x3 } };

// Fog (6).
constexpr uint32_t g_ps_0xEB7BE1D6_hash = 0xEB7BE1D6;
constexpr GUID g_ps_0xEB7BE1D6_guid = { 0x9eaab4f0, 0x8224, 0x4a26, { 0xa4, 0x11, 0x2a, 0x98, 0x9f, 0x7d, 0xb8, 0xaf } };

// Fog (7).
constexpr uint32_t g_ps_0xF5E21918_hash = 0xF5E21918;
constexpr GUID g_ps_0xF5E21918_guid = { 0x973eae08, 0x71f3, 0x492a, { 0x94, 0xe7, 0x94, 0xb, 0x15, 0xe8, 0x7b, 0xbc } };

// Ceiling debris.
constexpr uint32_t g_ps_0xDC232D31_hash = 0xDC232D31;
constexpr GUID g_ps_0xDC232D31_guid = { 0xa7cc6654, 0xa644, 0x4908, { 0xb1, 0xf4, 0x6a, 0x9e, 0x59, 0x84, 0xf8, 0xb5 } };

// "R" glass door.
constexpr uint32_t g_ps_0x59018C97_hash = 0x59018C97;
constexpr GUID g_ps_0x59018C97_guid = { 0x83c202f5, 0x2883, 0x4f24, { 0x83, 0xdb, 0x1c, 0x6e, 0xb3, 0x72, 0x6, 0x7 } };

// Godrays.
constexpr uint32_t g_ps_0xE689FDF8_hash = 0xE689FDF8;
constexpr GUID g_ps_0xE689FDF8_guid = { 0xeda804e9, 0xb9ab, 0x487e, { 0xa9, 0x45, 0x6a, 0xf2, 0x6e, 0xd5, 0x81, 0xef } };

// Flashing images.
constexpr uint32_t g_ps_0x7862AA89_hash = 0x7862AA89;
constexpr GUID g_ps_0x7862AA89_guid = { 0xb012ee87, 0x13a1, 0x4c5f, { 0x8a, 0xc4, 0x5b, 0x14, 0x70, 0xc4, 0x66, 0x65 } };

// Bloom downsample.
constexpr uint32_t g_ps_0xB51C436B_hash = 0xB51C436B;
constexpr GUID g_ps_0xB51C436B_guid = { 0xf6b85c15, 0x7efe, 0x4504, { 0x9a, 0xa5, 0xd7, 0xa9, 0x31, 0x7, 0xc, 0xb3 } };
static Com_ptr<ID3D10PixelShader> g_ps_0xB51C436B;

// Bloom (1).
constexpr uint32_t g_ps_0xB1DCCAE7_hash = 0xB1DCCAE7;
constexpr GUID g_ps_0xB1DCCAE7_guid = { 0x83883084, 0x9396, 0x4368, { 0xac, 0x72, 0x12, 0x74, 0xe9, 0x65, 0x25, 0x2a } };

// Bloom (2).
constexpr uint32_t g_ps_0x1A782DB1_hash = 0x1A782DB1;
constexpr GUID g_ps_0x1A782DB1_guid = { 0x7f42b436, 0x268d, 0x4967, { 0xa5, 0xf, 0x0, 0x4b, 0x9f, 0x47, 0x5d, 0x45 } };

// Bloom (3).
constexpr uint32_t g_ps_0xBD24CC87_hash = 0xBD24CC87;
constexpr GUID g_ps_0xBD24CC87_guid = { 0x96e8e31a, 0x856c, 0x4044, { 0xa6, 0xc2, 0xda, 0x22, 0xfe, 0x9b, 0xe2, 0xd5 } };

// Tone mapping.
// The last shader before UI.
constexpr uint32_t g_ps_0x87A0B43D_hash = 0x87A0B43D;
constexpr GUID g_ps_0x87A0B43D_guid = { 0x476ef3b9, 0xe14e, 0x49f2, { 0xa6, 0x5c, 0x8c, 0x7a, 0x41, 0xd4, 0xa6, 0x12 } };
static Com_ptr<ID3D10PixelShader> g_ps_0x87A0B43D;

static Com_ptr<ID3D10ShaderResourceView> g_srv_depth;

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

// AMD FFX LFGA
static Com_ptr<ID3D10ShaderResourceView> g_srv_blue_noise_tex;
static Com_ptr<ID3D10PixelShader> g_ps_amd_ffx_lfga;
static float g_amd_ffx_lfga_amount = 0.4f;

static Com_ptr<ID3D10SamplerState> g_smp_point_wrap;
static Com_ptr<ID3D10SamplerState> g_smp_point_clamp;

// XeGTAO
constexpr int XE_GTAO_DEPTH_MIP_LEVELS = 5;
static XeGTAO_defines g_xegtao_defines;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_prefilter_depth_mip0;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_prefilter_depths;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_main_pass;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_denoise1_pass;
static Com_ptr<ID3D10PixelShader> g_ps_xegtao_denoise2_pass;
static Com_ptr<ID3D10BlendState> g_blend_xegtao;
static Com_ptr<ID3D10Buffer> g_cb_tonemap; // We only need depth linearization.
static bool g_xegtao_enable = true;
static float g_xegtao_fov_y = 47.0f; // in degrees
static float g_xegtao_radius = 0.4f;
static int g_xegtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
static bool is_xegtao_drawn;

// Bloom
static Com_ptr<ID3D10PixelShader> g_ps_bloom_prefilter;
static Com_ptr<ID3D10PixelShader> g_ps_bloom_downsample;
static Com_ptr<ID3D10PixelShader> g_ps_bloom_upsample;
static Com_ptr<ID3D10BlendState> g_blend_bloom;
static Com_ptr<ID3D10Buffer> g_cb_bloom;
static int g_bloom_nmips = 6;
static std::vector<float> g_bloom_sigmas;

// enum TRC_
static int g_trc = TRC_GAMMA;

static float g_gamma = 2.2f;

static float g_bloom_intensity = 1.0f;

// FPS Limiter
//

// Exposed to user.
static float g_user_set_fps_limit = 240.0f; // in FPS
static int g_user_set_accounted_error = 2; // in ms

// Internal only.
static std::chrono::duration<double> g_frame_interval; // in seconds
static std::chrono::duration<double> g_accounted_error; // in seconds

//

// ReShade API can't properly handle some things that we need.
// Examples:
// - Rebinding back buffer.
// - DXGI_PRESENT_ALLOW_TEARING flag, even it should.
// - FPS limiter gets halved unless we do some hacks.
// TODO: Recheck can we just use ReShade API instead of our own hook.
typedef HRESULT(__stdcall *IDXGISwapChain__Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static IDXGISwapChain__Present g_original_present;

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

static void create_vertex_shader(ID3D10Device* device, ID3D10VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "vs_4_1", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), vs), >= 0);
}

static void create_pixel_shader(ID3D10Device* device, ID3D10PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "ps_4_1", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), ps), >= 0);
}

static void create_constant_buffer(ID3D10Device* device, ID3D10Buffer** buffer, UINT size)
{
	D3D10_BUFFER_DESC buffer_desc = {};
	buffer_desc.ByteWidth = size;
	buffer_desc.Usage = D3D10_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	ensure(device->CreateBuffer(&buffer_desc, nullptr, buffer), >= 0);
}

static void update_constant_buffer(ID3D10Buffer* buffer, void* data, size_t size)
{
	void* mapped;
	ensure(buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped), >= 0);
	std::memcpy(mapped, data, size);
	buffer->Unmap();
}

// Used by AMD FFX LFGA.
static void create_srv_blue_noise_tex(ID3D10Device* device, ID3D10ShaderResourceView** srv)
{
	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = BLUE_NOISE_TEX_WIDTH;
	tex_desc.Height = BLUE_NOISE_TEX_HEIGHT;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = BLUE_NOISE_TEX_ARRAY_SIZE;
	tex_desc.Format = DXGI_FORMAT_R8_UNORM;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.Usage = D3D10_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	std::array<D3D10_SUBRESOURCE_DATA, BLUE_NOISE_TEX_ARRAY_SIZE> subresource_data;
	for (size_t i = 0; i < subresource_data.size(); ++i) {
		subresource_data[i].pSysMem = BLUE_NOISE_TEX + i * BLUE_NOISE_TEX_PITCH * BLUE_NOISE_TEX_HEIGHT;
		subresource_data[i].SysMemPitch = BLUE_NOISE_TEX_PITCH;
	}
	Com_ptr<ID3D10Texture2D> tex;
	ensure(device->CreateTexture2D(&tex_desc, subresource_data.data(), tex.put()), >= 0);
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv), >= 0);
}

// Used by AMD FFX LFGA.
static void create_sampler_point_wrap(ID3D10Device* device, ID3D10SamplerState** smp)
{
	auto sampler_desc = default_D3D10_SAMPLER_DESC();
	sampler_desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
	ensure(device->CreateSamplerState(&sampler_desc, smp), >= 0);
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
	g_ps_xegtao_denoise1_pass.reset();
	g_ps_xegtao_denoise2_pass.reset();
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
	device->VSGetShader(vs_original.put());

	// Viewports.
	UINT nviewports;
	device->RSGetViewports(&nviewports, nullptr);
	std::vector<D3D10_VIEWPORT> viewports_original(nviewports);
	device->RSGetViewports(&nviewports, viewports_original.data());

	// Rasterizer.
	Com_ptr<ID3D10RasterizerState> rasterizer_original;
	device->RSGetState(rasterizer_original.put());

	// PS.
	Com_ptr<ID3D10PixelShader> ps_original;
	device->PSGetShader(ps_original.put());
	std::array<ID3D10ShaderResourceView*, 2> srvs_original;
	device->PSGetShaderResources(0, srvs_original.size(), srvs_original.data());
	Com_ptr<ID3D10SamplerState> smp_original;
	device->PSGetSamplers(0, 1, smp_original.put());

	// Blend.
	Com_ptr<ID3D10BlendState> blend_original;
	FLOAT blend_factor_original[4];
	UINT sample_mask_original;
	device->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

	// RTs.
	Com_ptr<ID3D10RenderTargetView> rtv_original;
	Com_ptr<ID3D10DepthStencilView> dsv_original;
	device->OMGetRenderTargets(1, rtv_original.put(), dsv_original.put());
	device->OMSetRenderTargets(0, nullptr, nullptr);

	//

	// Set common states.
	//

	device->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create and bind Fullscreen Triangle VS.
	[[unlikely]] if (!g_vs_fullscreen_triangle) {
		create_vertex_shader(device, g_vs_fullscreen_triangle.put(), L"FullscreenTriangle_vs.hlsl");
	}
	device->VSSetShader(g_vs_fullscreen_triangle.get());

	device->RSSetState(nullptr);
	device->PSSetConstantBuffers(12, 1, &g_cb_tonemap);

	// Create and bind point clamp sampler.
	[[unlikely]] if (!g_smp_point_clamp) {
		create_sampler_point_clamp(device, g_smp_point_clamp.put());
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
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

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
		ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, rtv_working_depth_mips[i].put()), >= 0);
		srv_desc.Texture2D.MostDetailedMip = i;
		ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, srv_working_depth_mips[i].put()), >= 0);
	}

	// Create working depth SRV with all mips.
	Com_ptr<ID3D10ShaderResourceView> srv_working_depth;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_working_depth.put()), >= 0);

	// Prefilter depths viewport, for mip0.
	D3D10_VIEWPORT viewport = {};
	viewport.Width = g_swapchain_width;
	viewport.Height = g_swapchain_height;

	// Create prefilter depths PS, for mip0.
	[[unlikely]] if (!g_ps_xegtao_prefilter_depth_mip0) {
		g_xegtao_defines.set(g_swapchain_width, g_swapchain_height, g_xegtao_fov_y, g_xegtao_radius, g_xegtao_quality);
		create_pixel_shader(device, g_ps_xegtao_prefilter_depth_mip0.put(), L"XeGTAO.hlsl", "prefilter_depths_mip0_ps", g_xegtao_defines.get());
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
		create_pixel_shader(device, g_ps_xegtao_prefilter_depths.put(), L"XeGTAO.hlsl", "prefilter_depths_ps", g_xegtao_defines.get());
	}
	device->PSSetShader(g_ps_xegtao_prefilter_depths.get());

	// Prefilter depths, mips 1 to 4.
	for (UINT i = 1; i < 5; ++i) {
		viewport.Width = std::max(1u, g_swapchain_width >> i);
		viewport.Height = std::max(1u, g_swapchain_height >> i);

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
		create_pixel_shader(device, g_ps_xegtao_main_pass.put(), L"XeGTAO.hlsl", "main_pass_ps", g_xegtao_defines.get());
	}

	// Create RT views.
	tex_desc.MipLevels = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
	Com_ptr<ID3D10RenderTargetView> rtv_main_pass;
	ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_main_pass.put()), >= 0);
	Com_ptr<ID3D10ShaderResourceView> srv_main_pass;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_main_pass.put()), >= 0);

	// Bindings.
	device->OMSetRenderTargets(1, &rtv_main_pass, nullptr);
	device->PSSetShader(g_ps_xegtao_main_pass.get());
	device->PSSetShaderResources(0, 1, &srv_working_depth);

	device->Draw(3, 0);

	//

	// Doing 2 XeGTAODenoisePass passes correspond to "Denoising level: Medium" from the XeGTAO demo.

	// DenoisePass1 pass.
	//

	// Create PS.
	[[unlikely]] if (!g_ps_xegtao_denoise1_pass) {
		create_pixel_shader(device, g_ps_xegtao_denoise1_pass.put(), L"XeGTAO.hlsl", "denoise_pass_ps");
	}

	// Create RT views.
	ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
	Com_ptr<ID3D10RenderTargetView> rtv_denoise1_pass;
	ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_denoise1_pass.put()), >= 0);
	Com_ptr<ID3D10ShaderResourceView> srv_denoise1_pass;
	ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_denoise1_pass.put()), >= 0);

	// Bindings.
	device->OMSetRenderTargets(1, &rtv_denoise1_pass, nullptr);
	device->PSSetShader(g_ps_xegtao_denoise1_pass.get());
	device->PSSetShaderResources(1, 1, &srv_main_pass);

	device->Draw(3, 0);

	//

	// DenoisePass2 pass
	//

	// Create PS.
	[[unlikely]] if (!g_ps_xegtao_denoise2_pass) {
		const D3D10_SHADER_MACRO defines[] = {
			{ "FINAL_APPLY", "" },
			{ nullptr, nullptr }
		};
		create_pixel_shader(device, g_ps_xegtao_denoise2_pass.put(), L"XeGTAO.hlsl", "denoise_pass_ps", defines);
	}

	// Create blend.
	[[unlikely]] if (!g_blend_xegtao) {
		auto blend_desc = default_D3D10_BLEND_DESC();
		blend_desc.BlendEnable[0] = TRUE;
		blend_desc.SrcBlend = D3D10_BLEND_DEST_COLOR;
		ensure(device->CreateBlendState(&blend_desc, g_blend_xegtao.put()), >= 0);
	}

	// Bindings.
	////

	device->OMSetRenderTargets(1, rtv, nullptr);
	device->PSSetShader(g_ps_xegtao_denoise2_pass.get());
	device->PSSetShaderResources(1, 1, &srv_denoise1_pass);

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

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	is_xegtao_drawn = false;

	// We have to rebind back buffer and DS later for flip mode to work in this game.
	Com_ptr<ID3D10Device> device;
	ensure(swapchain->GetDevice(IID_PPV_ARGS(device.put())), >= 0);
	Com_ptr<ID3D10RenderTargetView> rtv;
	Com_ptr<ID3D10DepthStencilView> dsv;
	device->OMGetRenderTargets(1, rtv.put(), dsv.put());

	// Limit FPS
	//

	static std::chrono::high_resolution_clock::time_point start;

	// We need to account for the acctual frame time.
	const auto sleep_time = g_frame_interval - std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start);

	// Precise sleep
	////

	const auto sleep_start = std::chrono::high_resolution_clock::now();

	// Looks like we don't need to set timeBeginPeriod(1) for the best accuracy. 
	std::this_thread::sleep_for(sleep_time - g_accounted_error);

	while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - sleep_start) < sleep_time) {
		continue;
	}

	////

	start = std::chrono::high_resolution_clock::now();

	//

	flags |= sync_interval ? 0 : DXGI_PRESENT_ALLOW_TEARING;
	auto hr = g_original_present(swapchain, sync_interval, flags);

	// Rebind back buffer and DS.
	device->OMSetRenderTargets(1, &rtv, dsv.get());

	return hr;
}

static bool on_draw_indexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	// Here, for now we only have XeGTAO, so optimize with early exit.
	if (!g_xegtao_enable || is_xegtao_drawn) {
		return false;
	}

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(ps.put());
	if (!ps) {
		return false;
	}

	// We can't reliably track shaders that we need via handle alone,
	// so we will use private data.
	//
	// The order doesn't matter, this will be equivalent to the logical OR.

	// 0xB69D6558 fog (1)
	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_0xB69D6558_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xB69D6558_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xE2683E33 fog (2)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xE2683E33_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xE2683E33_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xC907CF9A fog (3)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xC907CF9A_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xC907CF9A_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0x9CFB96DA fog (4)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x9CFB96DA_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x9CFB96DA_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0x3B177042 fog (5)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x3B177042_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x3B177042_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xEB7BE1D6 fog (6)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xEB7BE1D6_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xEB7BE1D6_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xF5E21918 fog (7)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xF5E21918_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xF5E21918_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xDC232D31 ceiling debris
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xDC232D31_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xDC232D31_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_xegtao(device, rtv_original.put());
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	// 0x59018C97 "R" glass door
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x59018C97_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x59018C97_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0xE689FDF8 godrays
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xE689FDF8_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xE689FDF8_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
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

	// 0x7862AA89 flashing images
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x7862AA89_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x7862AA89_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		#if DEV
		// Get RT resource (texture) and texture description.
		// Make sure we always have the main scene.
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			log_debug("0x7862AA89 RTV wasnt what we expected it to be.");
			return false;
		}
		#endif

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		return true;
	}

	return false;
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto device = (ID3D10Device*)cmd_list->get_native();
	Com_ptr<ID3D10PixelShader> ps;
	device->PSGetShader(ps.put());
	if (!ps) {
		return false;
	}

	// We can't reliably track shaders that we need via handle alone,
	// so we will use private data.
	//
	// The order doesn't matter, this will be equivalent to the logical OR.

	// 0xE2683E33 fog (2)
	uint32_t hash;
	UINT size = sizeof(hash);
	auto hr = ps->GetPrivateData(g_ps_0xE2683E33_guid, &size, &hash);
	if (g_xegtao_enable && !is_xegtao_drawn && SUCCEEDED(hr) && hash == g_ps_0xE2683E33_hash) {
		// We expect RTV to be the main scene.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		// Get RT resource (texture) and texture description.
		// We won't always have the main scene, check via resorce dimensions do we have it. 
		Com_ptr<ID3D10Resource> resource;
		rtv_original->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);
		if (tex_desc.Width != g_swapchain_width) {
			return false;
		}

		draw_xegtao(device, &rtv_original);
		is_xegtao_drawn = true;

		// Draw the original shader.
		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		return true;
	}

	// 0xB51C436B bloom downsample
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xB51C436B_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xB51C436B_hash) {
		if (g_xegtao_enable && !is_xegtao_drawn) {
			// We expect SRV to be the main scene.
			Com_ptr<ID3D10ShaderResourceView> srv_original;
			device->PSGetShaderResources(0, 1, srv_original.put());

			// Get the resource and create RTV.
			Com_ptr<ID3D10Resource> resource;
			srv_original->GetResource(resource.put());
			Com_ptr<ID3D10RenderTargetView> rtv;
			ensure(device->CreateRenderTargetView(resource.get(), nullptr, rtv.put()), >= 0);

			draw_xegtao(device, &rtv);
			is_xegtao_drawn = true;
		}
		device->PSGetConstantBuffers(0, 1, g_cb_bloom.put());
		return true;
	}

	// Bloom (1)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xB1DCCAE7_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xB1DCCAE7_hash) {
		return true;
	}

	// Bloom (2)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x1A782DB1_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x1A782DB1_hash) {
		return true;
	}

	// Bloom (3)
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0xBD24CC87_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0xBD24CC87_hash) {
		return true;
	}

	// 0x87A0B43D tone mapping
	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_0x87A0B43D_guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_0x87A0B43D_hash) {

		device->PSGetConstantBuffers(0, 1, g_cb_tonemap.put());

		// We expect RTV to be a back buffer.
		Com_ptr<ID3D10RenderTargetView> rtv_original;
		device->OMGetRenderTargets(1, rtv_original.put(), nullptr);

		#if DEV && SHOW_AO
		draw_xegtao(device, &rtv_original);
		return true;
		#endif
		
		Com_ptr<ID3D10VertexShader> vs_original;
		device->VSGetShader(vs_original.put());

		// We expect SRV0 to be the scene.
		Com_ptr<ID3D10ShaderResourceView> srv_scene;
		device->PSGetShaderResources(0, 1, srv_scene.put());

		// Get scenes texture description.
		Com_ptr<ID3D10Resource> resource;
		srv_scene->GetResource(resource.put());
		Com_ptr<ID3D10Texture2D> tex;
		ensure(resource->QueryInterface(tex.put()), >= 0);
		D3D10_TEXTURE2D_DESC tex_desc;
		tex->GetDesc(&tex_desc);

		#if DEV
		// Primitive topology should be D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST.
		D3D10_PRIMITIVE_TOPOLOGY primitive_topology;
		device->IAGetPrimitiveTopology(&primitive_topology);
		if (primitive_topology != D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
			log_debug("The expected primitive topology wasn't what we expected it to be!");
		}

		// Sampler in slot 0 should be:
		// D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		Com_ptr<ID3D10SamplerState> smp;
		device->PSGetSamplers(0, 1, smp.put());
		D3D10_SAMPLER_DESC smp_desc;
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 0 wasn't what we expected it to be!");
		}

		// Sampler in slot 1 should be:
		// D3D10_FILTER_MIN_MAG_MIP_LINEAR
		// D3D10_TEXTURE_ADDRESS_CLAMP
		// MipLODBias 0, MinLOD 0, MaxLOD FLT_MAX
		// D3D10_COMPARISON_NEVER
		device->PSGetSamplers(1, 1, smp.put());
		smp->GetDesc(&smp_desc);
		if (smp_desc.Filter != D3D10_FILTER_MIN_MAG_MIP_LINEAR || smp_desc.AddressU != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressV != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.AddressW != D3D10_TEXTURE_ADDRESS_CLAMP || smp_desc.MipLODBias != 0.0f || smp_desc.MinLOD != 0.0f || smp_desc.MaxLOD != D3D10_FLOAT32_MAX || smp_desc.ComparisonFunc != D3D10_COMPARISON_NEVER) {
			log_debug("The expected sampler in the slot 1 wasn't what we expected it to be!");
		}
		#endif // DEV

		// Bloom
		//

		// Create Y MIPs and views.
		tex_desc.Width /= 2;
		tex_desc.Height /= 2;
		tex_desc.MipLevels = g_bloom_nmips;
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		std::vector<ID3D10RenderTargetView*> rtv_mips_y(g_bloom_nmips);
		std::vector<ID3D10ShaderResourceView*> srv_mips_y(g_bloom_nmips);
		D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = tex_desc.Format;
		rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = tex_desc.Format;
		srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		for (int i = 0; i < g_bloom_nmips; ++i) {
		    rtv_desc.Texture2D.MipSlice = i;
		    ensure(device->CreateRenderTargetView(tex.get(), &rtv_desc, &rtv_mips_y[i]), >= 0);
		    srv_desc.Texture2D.MostDetailedMip = i;
		    ensure(device->CreateShaderResourceView(tex.get(), &srv_desc, &srv_mips_y[i]), >= 0);
		}

		const UINT y_mip0_width = tex_desc.Width;
		const UINT y_mip0_height = tex_desc.Height;

		std::vector<ID3D10RenderTargetView*> rtv_mips_x(g_bloom_nmips);
		std::vector<ID3D10ShaderResourceView*> srv_mips_x(g_bloom_nmips);
		
		// Create X MIP0 and views.
		tex_desc.Width = g_swapchain_width / 2;
		tex_desc.Height = g_swapchain_height;
		tex_desc.MipLevels = 1;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_mips_x[0]), >= 0);
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_mips_x[0]), >= 0);

		const UINT x_mip0_width = tex_desc.Width;
		const UINT x_mip0_height = tex_desc.Height;

		// Create rest of the X MIPs and views.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			tex_desc.Width = std::max(1u, x_mip0_width >> i);
			tex_desc.Height = std::max(1u, x_mip0_height >> i);
			ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(device->CreateRenderTargetView(tex.get(), nullptr, &rtv_mips_x[i]), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, &srv_mips_x[i]), >= 0);
		}

		// Prefilter + downsample pass
		////

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, g_vs_fullscreen_triangle.put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create downsample PS.
		[[unlikely]] if (!g_ps_bloom_downsample) {
			create_pixel_shader(device, g_ps_bloom_downsample.put(), L"Bloom_ps.hlsl", "bloom_downsample_ps");
		}

		// Create prefilter PS.
		[[unlikely]] if (!g_ps_bloom_prefilter) {
			create_pixel_shader(device, g_ps_bloom_prefilter.put(), L"Bloom_ps.hlsl", "bloom_prefilter_ps");
		}

		// Create constant buffer.
		[[unlikely]] if (!g_cb_graphical_upgrade) {
			create_constant_buffer(device, g_cb_graphical_upgrade.put(), sizeof(g_cb_garphical_upgrade_data));
		}

		D3D10_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_garphical_upgrade_data.src_size = float2(g_swapchain_width, g_swapchain_height);
		g_cb_garphical_upgrade_data.inv_src_size.x = 1.0f / g_cb_garphical_upgrade_data.src_size.x;
		g_cb_garphical_upgrade_data.inv_src_size.y = 1.0f / g_cb_garphical_upgrade_data.src_size.y;
		g_cb_garphical_upgrade_data.axis = float2(1.0f, 0.0f);
		g_cb_garphical_upgrade_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_mips_x[0], nullptr);
		device->VSSetShader(g_vs_fullscreen_triangle.get());
		device->PSSetShader(g_ps_bloom_downsample.get());
		device->PSSetConstantBuffers(12, 1, &g_cb_bloom);
		device->PSSetConstantBuffers(13, 1, &g_cb_graphical_upgrade);
		device->PSSetShaderResources(0, 1, &srv_scene);
		device->RSSetViewports(1, &viewport_x);

		// Draw X pass.
		device->Draw(3, 0);

		std::vector<D3D10_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Update CB.
		g_cb_garphical_upgrade_data.src_size = float2(x_mip0_width, x_mip0_height);
		g_cb_garphical_upgrade_data.axis = float2(0.0f, 1.0f);
		g_cb_garphical_upgrade_data.inv_src_size.x = 1.0f / g_cb_garphical_upgrade_data.src_size.x;
		g_cb_garphical_upgrade_data.inv_src_size.y = 1.0f / g_cb_garphical_upgrade_data.src_size.y;
		update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_mips_y[0], nullptr);
		device->PSSetShader(g_ps_bloom_prefilter.get());
		device->PSSetShaderResources(0, 1, &srv_mips_x[0]);
		device->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		device->Draw(3, 0);

		////

		// Downsample passes
		////

		// Common bindings.
		device->PSSetShader(g_ps_bloom_downsample.get());

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_garphical_upgrade_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_garphical_upgrade_data.axis = float2(1.0f, 0.0f);
			g_cb_garphical_upgrade_data.inv_src_size.x = 1.0f / g_cb_garphical_upgrade_data.src_size.x;
			g_cb_garphical_upgrade_data.inv_src_size.y = 1.0f / g_cb_garphical_upgrade_data.src_size.y;
			g_cb_garphical_upgrade_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));

			// Bindings.
		    device->OMSetRenderTargets(1, &rtv_mips_x[i], nullptr);
		    device->PSSetShaderResources(0, 1, &srv_mips_y[i - 1]);
		    device->RSSetViewports(1, &viewport_x);

			// Draw X pass.
		    device->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_garphical_upgrade_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_garphical_upgrade_data.axis = float2(0.0f, 1.0f);
			g_cb_garphical_upgrade_data.inv_src_size.x = 1.0f / g_cb_garphical_upgrade_data.src_size.x;
			g_cb_garphical_upgrade_data.inv_src_size.y = 1.0f / g_cb_garphical_upgrade_data.src_size.y;
			update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));

			// Bindings.
		    device->OMSetRenderTargets(1, &rtv_mips_y[i], nullptr);
		    device->PSSetShaderResources(0, 1, &srv_mips_x[i]);
		    device->RSSetViewports(1, &viewports_y[i]);

			// Draw Y pass.
		    device->Draw(3, 0);
		}

		////

		// Upsample passes
		////

		// Create PS.
		[[unlikely]] if (!g_ps_bloom_upsample) {
			create_pixel_shader(device, g_ps_bloom_upsample.put(), L"Bloom_ps.hlsl", "bloom_upsample_ps");
		}

		// Create blend.
		[[unlikely]] if (!g_blend_bloom) {
			auto blend_desc = default_D3D10_BLEND_DESC();
			blend_desc.BlendEnable[0] = TRUE;
			blend_desc.SrcBlend = D3D10_BLEND_BLEND_FACTOR;
			blend_desc.DestBlend = D3D10_BLEND_BLEND_FACTOR;
			ensure(device->CreateBlendState(&blend_desc, g_blend_bloom.put()), >= 0);
		}

		// Common bindings.
		device->PSSetShader(g_ps_bloom_upsample.get());

		// Render upsample passes.
		for (int i = g_bloom_nmips - 1; i > 0; --i) {

			// If both dst and src are D3D10_BLEND_BLEND_FACTOR
			// factor of 0.5 is enegrgy preserving.
			static constexpr float blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.0f };

			// Update CB.
			g_cb_garphical_upgrade_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_garphical_upgrade_data.inv_src_size.x = 1.0f / g_cb_garphical_upgrade_data.src_size.x;
			g_cb_garphical_upgrade_data.inv_src_size.y = 1.0f / g_cb_garphical_upgrade_data.src_size.y;
			update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));

			// Bindings.
			device->OMSetRenderTargets(1, &rtv_mips_y[i - 1], nullptr);
		    device->PSSetShaderResources(0, 1, &srv_mips_y[i]);
		    device->RSSetViewports(1, &viewports_y[i - 1]);
			device->OMSetBlendState(g_blend_bloom.get(), blend_factor, UINT_MAX);

		    device->Draw(3, 0);
		}

		////

		//

		// 0x87A0B43D pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_0x87A0B43D) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_ps_0x87A0B43D.put(), L"0x87A0B43D_ps.hlsl", "main", defines);
		}

		// Create RTs and views.
		////

		tex_desc.Width = g_swapchain_width;
		tex_desc.Height = g_swapchain_height;
		tex_desc.MipLevels = 1;

		// Linear.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_0x87A0B43D_linear;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_0x87A0B43D_linear.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_0x87A0B43D_linear;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_0x87A0B43D_linear.put()), >= 0);

		// Delinearized.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_0x87A0B43D_delinearized;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_0x87A0B43D_delinearized.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_0x87A0B43D_delinearized;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_0x87A0B43D_delinearized.put()), >= 0);

		////

		D3D10_VIEWPORT viewport = {};
		viewport.Width = tex_desc.Width;
		viewport.Height = tex_desc.Height;

		// Bindings.
		const std::array rtvs_0x87A0B43D = { rtv_0x87A0B43D_linear.get(), rtv_0x87A0B43D_delinearized.get() };
		device->OMSetRenderTargets(rtvs_0x87A0B43D.size(), rtvs_0x87A0B43D.data(), nullptr);
		device->VSSetShader(vs_original.get());
		device->PSSetShader(g_ps_0x87A0B43D.get());
		const std::array srvs_0x87A0B43D = { srv_scene.get(), srv_mips_y[0] };
		device->PSSetShaderResources(0, srvs_0x87A0B43D.size(), srvs_0x87A0B43D.data());
		device->RSSetViewports(1, &viewport);
		device->OMSetBlendState(nullptr, nullptr, UINT_MAX);

		cmd_list->draw(vertex_count, instance_count, first_vertex, first_instance);

		//

		// SMAAEdgeDetection pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_edge_detection) {
			g_smaa_rt_metrics.set(tex_desc.Width, tex_desc.Height);
			create_vertex_shader(device, g_vs_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_edge_detection) {
			create_pixel_shader(device, g_ps_smaa_edge_detection.put(), L"SMAA_impl.hlsl", "smaa_edge_detection_ps", g_smaa_rt_metrics.get());
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_replace_stencil) {
			D3D10_DEPTH_STENCIL_DESC desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_REPLACE;
			ensure(device->CreateDepthStencilState(&desc, g_ds_smaa_disable_depth_replace_stencil.put()), >= 0);
		}

		// Create DSV.
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10DepthStencilView> dsv;
		ensure(device->CreateDepthStencilView(tex.get(), nullptr, dsv.put()), >= 0);

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
		tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_edge_detection;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_edge_detection.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_edge_detection;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_edge_detection.put()), >= 0);

		// Bindings.
		device->OMSetDepthStencilState(g_ds_smaa_disable_depth_replace_stencil.get(), 1);
		device->OMSetRenderTargets(1, &rtv_smaa_edge_detection, dsv.get());
		device->VSSetShader(g_vs_smaa_edge_detection.get());
		device->PSSetShader(g_ps_smaa_edge_detection.get());
		const std::array ps_srvs_smaa_edge_detection = { srv_0x87A0B43D_delinearized.get(), g_srv_depth.get() };
		device->PSSetShaderResources(0, ps_srvs_smaa_edge_detection.size(), ps_srvs_smaa_edge_detection.data());

		device->ClearRenderTargetView(rtv_smaa_edge_detection.get(), g_smaa_clear_color);
		device->ClearDepthStencilView(dsv.get(), D3D10_CLEAR_STENCIL, 1.0f, 0);
		device->Draw(3, 0);

		//

		// SMAABlendingWeightCalculation pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_smaa_blending_weight_calculation) {
			create_vertex_shader(device, g_vs_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_blending_weight_calculation) {
			create_pixel_shader(device, g_ps_smaa_blending_weight_calculation.put(), L"SMAA_impl.hlsl", "smaa_blending_weight_calculation_ps", g_smaa_rt_metrics.get());
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
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv_smaa_area_tex.put()), >= 0);
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
			ensure(device->CreateTexture2D(&tex_desc, &subresource_data, tex.put()), >= 0);
			ensure(device->CreateShaderResourceView(tex.get(), nullptr, g_srv_smaa_search_tex.put()), >= 0);
		}

		// Create DS.
		[[unlikely]] if (!g_ds_smaa_disable_depth_use_stencil) {
			auto desc = default_D3D10_DEPTH_STENCIL_DESC();
			desc.DepthEnable = FALSE;
			desc.StencilEnable = TRUE;
			desc.FrontFace.StencilFunc = D3D10_COMPARISON_EQUAL;
			ensure(device->CreateDepthStencilState(&desc, g_ds_smaa_disable_depth_use_stencil.put()), >= 0);
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_blending_weight_calculation;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_blending_weight_calculation.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_blending_weight_calculation;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_blending_weight_calculation.put()), >= 0);

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
			create_vertex_shader(device, g_vs_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_vs", g_smaa_rt_metrics.get());
		}

		// Create PS.
		[[unlikely]] if (!g_ps_smaa_neighborhood_blending) {
			create_pixel_shader(device, g_ps_smaa_neighborhood_blending.put(), L"SMAA_impl.hlsl", "smaa_neighborhood_blending_ps", g_smaa_rt_metrics.get());
		}

		// Create RT and views.
		tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_smaa_neighborhood_blending;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_smaa_neighborhood_blending.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_smaa_neighborhood_blending;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_smaa_neighborhood_blending.put()), >= 0);

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_smaa_neighborhood_blending, nullptr);
		device->VSSetShader(g_vs_smaa_neighborhood_blending.get());
		device->PSSetShader(g_ps_smaa_neighborhood_blending.get());
		const std::array ps_srvs_neighborhood_blending = { srv_0x87A0B43D_linear.get(), srv_smaa_blending_weight_calculation.get() };
		device->PSSetShaderResources(0, ps_srvs_neighborhood_blending.size(), ps_srvs_neighborhood_blending.data());

		device->Draw(3, 0);

		//

		// AMD FFX CAS pass
		//

		// Create VS.
		[[unlikely]] if (!g_vs_fullscreen_triangle) {
			create_vertex_shader(device, g_vs_fullscreen_triangle.put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_ps_amd_ffx_cas) {
			const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
			const D3D10_SHADER_MACRO defines[] = {
				{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_ps_amd_ffx_cas.put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
		}

		// Create RT and views.
		ensure(device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
		Com_ptr<ID3D10RenderTargetView> rtv_amd_ffx_cas;
		ensure(device->CreateRenderTargetView(tex.get(), nullptr, rtv_amd_ffx_cas.put()), >= 0);
		Com_ptr<ID3D10ShaderResourceView> srv_amd_ffx_cas;
		ensure(device->CreateShaderResourceView(tex.get(), nullptr, srv_amd_ffx_cas.put()), >= 0);

		// Bindings.
		device->OMSetRenderTargets(1, &rtv_amd_ffx_cas, nullptr);
		device->VSSetShader(g_vs_fullscreen_triangle.get());
		device->PSSetShader(g_ps_amd_ffx_cas.get());
		device->PSSetShaderResources(0, 1, &srv_smaa_neighborhood_blending);

		device->Draw(3, 0);

		//

		// AMD FFX LFGA pass
		//

		// Create PS.
		[[unlikely]] if (!g_ps_amd_ffx_lfga) {
			const std::string viewport_dims = std::format("float2({},{})", (float)tex_desc.Width, (float)tex_desc.Height);
			const std::string amd_ffx_lfga_amount_str = std::to_string(g_amd_ffx_lfga_amount);
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D10_SHADER_MACRO defines[] = {
				{ "WIEWPORT_DIMS", viewport_dims.c_str() },
				{ "AMOUNT", amd_ffx_lfga_amount_str.c_str() },
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(device, g_ps_amd_ffx_lfga.put(), L"AMD_FFX_LFGA_ps.hlsl", "main", defines);
		}

		// Create constant buffer.
		[[unlikely]] if (!g_cb_graphical_upgrade) {
			create_constant_buffer(device, g_cb_graphical_upgrade.put(), sizeof(g_cb_garphical_upgrade_data));
		}

		// Create blue noise texture.
		[[unlikely]] if (!g_srv_blue_noise_tex) {
			create_srv_blue_noise_tex(device, g_srv_blue_noise_tex.put());
		}

		// Create point wrap sampler.
		[[unlikely]] if (!g_smp_point_wrap) {
			create_sampler_point_wrap(device, g_smp_point_wrap.put());
		}

		// Update the constant buffer.
		// We need to limit the temporal grain update rate, otherwise grain will flicker.
		constexpr auto interval = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(1.0 / (double)BLUE_NOISE_TEX_ARRAY_SIZE));
		static auto last_update = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		if (now - last_update >= interval) {
			g_cb_garphical_upgrade_data.tex_noise_index += 1.0f;
			if (g_cb_garphical_upgrade_data.tex_noise_index >= (float)BLUE_NOISE_TEX_ARRAY_SIZE) {
				g_cb_garphical_upgrade_data.tex_noise_index = 0.0f;
			}
			update_constant_buffer(g_cb_graphical_upgrade.get(), &g_cb_garphical_upgrade_data, sizeof(g_cb_garphical_upgrade_data));
			last_update += interval;
		}

		// Bindings.
		////

		device->OMSetRenderTargets(1, &rtv_original, nullptr);
		device->PSSetShader(g_ps_amd_ffx_lfga.get());
		device->PSSetConstantBuffers(13, 1, &g_cb_graphical_upgrade); // The game should never be using CB slot 13.
		const std::array ps_srvs_amd_ffx_lfga = { srv_amd_ffx_cas.get(), g_srv_blue_noise_tex.get() };
		device->PSSetShaderResources(0, ps_srvs_amd_ffx_lfga.size(), ps_srvs_amd_ffx_lfga.data());

		// Backup the original sampler.
		Com_ptr<ID3D10SamplerState> smp_original0;
		device->PSGetSamplers(0, 1, smp_original0.put());

		device->PSSetSamplers(0, 1, &g_smp_point_wrap);

		////

		device->Draw(3, 0);

		//

		// Restore original states.
		device->PSSetSamplers(0, 1, &smp_original0);

		// Release com arrays.
		auto release_com_array = [](auto& array){ for (auto* p : array) if (p) p->Release(); };
		release_com_array(rtv_mips_y);
		release_com_array(srv_mips_y);
		release_com_array(rtv_mips_x);
		release_com_array(srv_mips_x);

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
				case g_ps_0x87A0B43D_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x87A0B43D_guid, sizeof(g_ps_0x87A0B43D_hash), &g_ps_0x87A0B43D_hash), >= 0);
					break;
				case g_ps_0xB69D6558_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xB69D6558_guid, sizeof(g_ps_0xB69D6558_hash), &g_ps_0xB69D6558_hash), >= 0);
					break;
				case g_ps_0xC907CF9A_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xC907CF9A_guid, sizeof(g_ps_0xC907CF9A_hash), &g_ps_0xC907CF9A_hash), >= 0);
					break;
				case g_ps_0xE689FDF8_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xE689FDF8_guid, sizeof(g_ps_0xE689FDF8_hash), &g_ps_0xE689FDF8_hash), >= 0);
					break;
				case g_ps_0xB51C436B_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xB51C436B_guid, sizeof(g_ps_0xB51C436B_hash), &g_ps_0xB51C436B_hash), >= 0);
					break;
				case g_ps_0x9CFB96DA_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x9CFB96DA_guid, sizeof(g_ps_0x9CFB96DA_hash), &g_ps_0x9CFB96DA_hash), >= 0);
					break;
				case g_ps_0x3B177042_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x3B177042_guid, sizeof(g_ps_0x3B177042_hash), &g_ps_0x3B177042_hash), >= 0);
					break;
				case g_ps_0xE2683E33_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xE2683E33_guid, sizeof(g_ps_0xE2683E33_hash), &g_ps_0xE2683E33_hash), >= 0);
					break;
				case g_ps_0xEB7BE1D6_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xEB7BE1D6_guid, sizeof(g_ps_0xEB7BE1D6_hash), &g_ps_0xEB7BE1D6_hash), >= 0);
					break;
				case g_ps_0x59018C97_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x59018C97_guid, sizeof(g_ps_0x59018C97_hash), &g_ps_0x59018C97_hash), >= 0);
					break;
				case g_ps_0xF5E21918_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xF5E21918_guid, sizeof(g_ps_0xF5E21918_hash), &g_ps_0xF5E21918_hash), >= 0);
					break;
				case g_ps_0xDC232D31_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xDC232D31_guid, sizeof(g_ps_0xDC232D31_hash), &g_ps_0xDC232D31_hash), >= 0);
					break;
				case g_ps_0x7862AA89_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x7862AA89_guid, sizeof(g_ps_0x7862AA89_hash), &g_ps_0x7862AA89_hash), >= 0);
					break;
				case g_ps_0xB1DCCAE7_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xB1DCCAE7_guid, sizeof(g_ps_0xB1DCCAE7_hash), &g_ps_0xB1DCCAE7_hash), >= 0);
					break;
				case g_ps_0x1A782DB1_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0x1A782DB1_guid, sizeof(g_ps_0x1A782DB1_hash), &g_ps_0x1A782DB1_hash), >= 0);
					break;
				case g_ps_0xBD24CC87_hash:
					ensure(((ID3D10PixelShader*)pipeline.handle)->SetPrivateData(g_ps_0xBD24CC87_guid, sizeof(g_ps_0xBD24CC87_hash), &g_ps_0xBD24CC87_hash), >= 0);
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
		ensure(native_device->CreateShaderResourceView((ID3D10Resource*)resource.handle, nullptr, g_srv_depth.put()), >= 0);
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

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	// Set the window to borderless fullscreen, the main intention.
	// Non fullscreen window with or without border can be still set from the game if first set as windowed mode.
	// Fixes UI (menus and loading screen) being pushed below lower screen border.
	// Fixes ReShade overlay being smudged/blured.
	SetWindowLongPtrW((HWND)hwnd, GWL_STYLE, WS_POPUP);
	SetWindowPos((HWND)hwnd, HWND_TOP, 0, 0, desc.back_buffer.texture.width, desc.back_buffer.texture.height, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
	desc.back_buffer_count = 2;
	desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.present_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	desc.fullscreen_state = false;
	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();

	// Hook IDXGISwapChain::Present
	if (!g_original_present) {
		auto vtable = *(void***)native_swapchain;
		ensure(MH_CreateHook(vtable[8], &detour_present, (void**)&g_original_present), == MH_OK);
		ensure(MH_EnableHook(vtable[8]), == MH_OK);
	}

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
	g_ps_amd_ffx_lfga.reset();
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
	g_cb_graphical_upgrade.reset();
	g_vs_fullscreen_triangle.reset();
	g_ps_0xB51C436B.reset();
	g_ps_0x87A0B43D.reset();
	g_srv_depth.reset();

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

	// AMD FFX CAS
	g_ps_amd_ffx_cas.reset();

	// AMD FFX LFGA
	g_srv_blue_noise_tex.reset();
	g_ps_amd_ffx_lfga.reset();

	g_smp_point_wrap.reset();
	g_smp_point_clamp.reset();

	// XeGTAO
	g_ps_xegtao_prefilter_depth_mip0.reset();
	g_ps_xegtao_prefilter_depths.reset();
	g_ps_xegtao_main_pass.reset();
	g_ps_xegtao_denoise1_pass.reset();
	g_ps_xegtao_denoise2_pass.reset();
	g_blend_xegtao.reset();

	// Bloom.
	g_ps_bloom_prefilter.reset();
	g_ps_bloom_downsample.reset();
	g_ps_bloom_upsample.reset();
	g_blend_bloom.reset();
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
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOQuality", g_xegtao_quality)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOQuality", g_xegtao_quality);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
	}
	if (!reshade::get_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
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
	#if DEV
	if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1.0, 10.0)) {
		g_bloom_sigmas.resize(g_bloom_nmips);
	}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	}
	#endif

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
	static constexpr std::array xegtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("XeGTAO Quality", &g_xegtao_quality, xegtao_quality_items.data(), xegtao_quality_items.size())) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "XeGTAOQuality", g_xegtao_quality);
		reset_xegtao();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();
	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "TRC", g_trc);
		g_ps_amd_ffx_lfga.reset();
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Gamma", g_gamma);
		g_ps_amd_ffx_lfga.reset();
	}
	ImGui::EndDisabled();
	ImGui::Spacing();
	if (ImGui::SliderFloat("Bloom Intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "BloomIntensity", g_bloom_intensity);
		g_ps_0x87A0B43D.reset();
	}
	ImGui::Spacing();
	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "Sharpness", g_amd_ffx_cas_sharpness);
		g_ps_amd_ffx_cas.reset();
	}
	ImGui::Spacing();
	if (ImGui::SliderFloat("Grain amount", &g_amd_ffx_lfga_amount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, "BioshockGraphicalUpgrade", "GrainAmount", g_amd_ffx_lfga_amount);
		g_ps_amd_ffx_lfga.reset();
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

extern "C" __declspec(dllexport) const char* NAME = "BioshockGrapicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "BioshockGrapicalUpgrade v6.1.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/BioshockGraphicalUpgrade";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}
			g_graphical_upgrade_path = get_shaders_path();
			read_config();
			g_bloom_sigmas.resize(g_bloom_nmips);
			assert(g_bloom_nmips == 6);
			g_bloom_sigmas[0] = 1.5f;
			g_bloom_sigmas[1] = 1.0f;
			g_bloom_sigmas[2] = 1.0f;
			g_bloom_sigmas[3] = 1.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;
			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::init_device>(on_init_device);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			MH_Uninitialize();
			break;
	}
	return TRUE;
}