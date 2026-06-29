#define DEV 0
#define OUTPUT_ASSEMBLY 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"
#include "minhook/include/MinHook.h"

extern "C" __declspec(dllexport) const char* NAME = "MonsterHunterWorldGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/MonsterHunterWorldGraphicalUpgrade";

struct alignas(16) CBViewProjection
{
	row_major float4x4 fViewProj;
	row_major float4x4 fView;
	row_major float4x4 fProj;
	row_major float4x4 fViewI;
	row_major float4x4 fProjI;
	row_major float4x4 fViewProjI;
	float3 fCameraPos;
	float padding1; // Added.
	float3 fCameraDir;
	float padding2; // Added.
	float3 fZToLinear;
	float fCameraNearClip;
	float fCameraFarClip;
	float fCameraTargetDist;
	float2 padding3; // Added.
	float4 fPassThrough;
	float3 fLODBasePos;
	float padding4; // Added.
	row_major float4x4 fViewProjPF;
	row_major float4x4 fViewProjIPF;
	row_major float4x4 fViewPF;
	row_major float4x4 fProjPF;
	row_major float4x4 fViewProjIViewProjPF;
	row_major float4x4 fNoJitterProj;
	row_major float4x4 fNoJitterViewProj;
	row_major float4x4 fNoJitterViewProjI;
	row_major float4x4 fNoJitterViewProjIViewProjPF;
	float2 fPassThroughCorrect;
	/*bool*/ uint bWideMonitor;
	float padding5;
};

// Shader hooks.
//

constexpr Shader_hash g_cs_shadows_ao_denoiser_0x9F9DDC10 = { 0x9F9DDC10, { 0xae710e15, 0xe5b, 0x438c, { 0x82, 0x9b, 0x12, 0xf6, 0x80, 0xc9, 0x9a, 0x70 }}};
constexpr Shader_hash g_ps_copy_0xBCA17DC2 = { 0xBCA17DC2, { 0xa0d5c757, 0x92db, 0x4b3f, { 0xa7, 0x28, 0xe0, 0x15, 0xeb, 0x19, 0x9a, 0x37 }}};

// Bloom.
constexpr Shader_hash g_ps_bloom_threshold_0xC1DD61BF = { 0xC1DD61BF, { 0x96820693, 0x8f89, 0x4730, { 0x98, 0xca, 0x65, 0xd4, 0x8d, 0x48, 0x47, 0x6f }}};
constexpr Shader_hash g_ps_bloom_0xF45A211E = { 0xF45A211E, { 0xe0d1c2cf, 0x1be7, 0x4428, { 0x96, 0x1c, 0x73, 0x22, 0xc3, 0x19, 0xe7, 0x8e }}};
constexpr Shader_hash g_ps_bloom_0xEF5ED883 = { 0xEF5ED883, { 0x1985e0da, 0x7c92, 0x4a92, { 0xbf, 0xc7, 0xf8, 0x97, 0xe9, 0x62, 0xb9, 0xfd }}};
constexpr Shader_hash g_ps_bloom_0xBA76C0BA = { 0xBA76C0BA, { 0xfa89c767, 0xe97e, 0x42a5, { 0x92, 0x7c, 0x6c, 0x2d, 0x5b, 0x7, 0x15, 0xdc }}};
constexpr Shader_hash g_ps_bloom_add_0x77963D88 = { 0x77963D88, { 0x3c1d2c26, 0x6331, 0x4bbf, { 0x8c, 0x8, 0x30, 0xa3, 0x7c, 0xed, 0x8a, 0x68 }}};

constexpr Shader_hash g_cs_taa_0xD84C4AF0 = { 0xD84C4AF0, { 0xa489935f, 0x61c1, 0x4ab7, { 0xb8, 0x2, 0xc6, 0xdc, 0x65, 0xa4, 0x28, 0x7c }}};

constexpr Shader_hash g_ps_tonemap_0xAC274871 = { 0xAC274871, { 0xa5dc78d2, 0xfa48, 0x47d3, { 0x83, 0xe1, 0x1f, 0xf3, 0x83, 0xf2, 0xd, 0x40 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D11Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;

// DLSS
constexpr int g_dlss_flags{
	NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
	NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
	NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
};
static NVSDK_NGX_DLSS_Hint_Render_Preset g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
static int g_user_set_dlss_preset;
static bool g_enable_dlss;
static bool g_dlss_status;
static float g_jitter_x;
static float g_jitter_y;

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

// Tone responce curve.
static TRC g_trc = TRC_SRGB;
static float g_gamma = 2.2f;

// Device reseources.
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_x;

// We need to change flags on present, ReShade's callback can't handle that.
typedef HRESULT(__stdcall *IDXGISwapChain__Present)(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags);
static IDXGISwapChain__Present g_original_present;

static HRESULT __stdcall detour_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags)
{
	// The game may do fake preset with DXGI_PRESENT_TEST flag
	// than right after do the actual present.
	// DXGI_PRESENT_ALLOW_TEARING may be set by ReShade,
	// it's incompatible with DXGI_PRESENT_TEST so we have to remove it.
	if (flags & DXGI_PRESENT_TEST) {
		flags &= ~DXGI_PRESENT_ALLOW_TEARING;
	}

	auto hr = g_original_present(swapchain, sync_interval, flags);

	return hr;
}

static bool on_draw(reshade::api::command_list* cmd_list, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11PixelShader> ps;
	ctx->PSGetShader(ps.put(), nullptr, nullptr);
	if (!ps) {
		return false;
	}

	#if DEV
	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_copy_0xBCA17DC2.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_copy_0xBCA17DC2.hash) {
		// The shader is used to copy various resources.
		// Expecting depth to be the last should be reliable? Needs testing!
		Com_ptr<ID3D11ShaderResourceView> srv;
		ctx->PSGetShaderResources(0, 1, srv.put());
		if (srv) {
			srv->GetResource(g_managed_resources.resources["depth"_h].put());
		}

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_threshold_0xC1DD61BF.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_threshold_0xC1DD61BF.hash) {
		ctx->PSGetConstantBuffers(3, 1, g_managed_resources.buffers["CBLuminance"_h].put());
		ctx->PSGetShaderResources(1, 1, g_managed_resources.shader_resource_views["gLuminanceBufferSRV"_h].put());
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xBA76C0BA.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xBA76C0BA.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xEF5ED883.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xEF5ED883.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0xF45A211E.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0xF45A211E.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_add_0x77963D88.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_add_0x77963D88.hash) {

		// Backup IA.
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology_original;
		ctx->IAGetPrimitiveTopology(&primitive_topology_original);

		// Backup VS.
		Com_ptr<ID3D11VertexShader> vs_original;
		ctx->VSGetShader(vs_original.put(), nullptr, nullptr);

		// Backup PS.
		Com_ptr<ID3D11PixelShader> ps_original;
		ctx->PSGetShader(ps_original.put(), nullptr, nullptr);
		Com_ptr<ID3D11Buffer> cb_orginal;
		ctx->PSGetConstantBuffers(13, 1, cb_orginal.put());
		Com_ptr<ID3D11SamplerState> ps_sampler_original;
		ctx->PSGetSamplers(0, 1, ps_sampler_original.put());
		Com_ptr<ID3D11ShaderResourceView> ps_srv_original;
		ctx->PSGetShaderResources(0, 1, ps_srv_original.put());

		// Backup Viewports.
		UINT num_viewports;
		ctx->RSGetViewports(&num_viewports, nullptr);
		std::vector<D3D11_VIEWPORT> viewports_original(num_viewports);
		ctx->RSGetViewports(&num_viewports, viewports_original.data());

		// Backup Rasterizer.
		Com_ptr<ID3D11RasterizerState> rasterizer_original;
		ctx->RSGetState(rasterizer_original.put());

		// Backup Blend.
		Com_ptr<ID3D11BlendState> blend_original;
		FLOAT blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Get RTV (should be scene) and create SRV from it's resource.
		Com_ptr<ID3D11RenderTargetView> rtv_scene;
		ctx->OMGetRenderTargets(1, rtv_scene.put(), nullptr);
		Com_ptr<ID3D11Resource> resource_scene;
		rtv_scene->GetResource(resource_scene.put());
		Com_ptr<ID3D11ShaderResourceView> srv_scene;
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		ensure(g_device->CreateShaderResourceView(resource_scene.get(), &srv_desc, srv_scene.put()), >= 0);

		// Create MIPs and views.
		//

		const UINT y_mip0_width = g_swapchain_width >> 1;
		const UINT y_mip0_height = g_swapchain_height >> 1;

		// Create Y MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_y[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = y_mip0_width;
			tex_desc.Height = y_mip0_height;
			tex_desc.MipLevels = g_bloom_nmips;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = tex_desc.Format;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = tex_desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;

			for (int i = 0; i < g_bloom_nmips; ++i) {
				rtv_desc.Texture2D.MipSlice = i;
				ensure(g_device->CreateRenderTargetView(tex.get(), &rtv_desc, &g_rtv_bloom_mips_y[i]), >= 0);
				srv_desc.Texture2D.MostDetailedMip = i;
				ensure(g_device->CreateShaderResourceView(tex.get(), &srv_desc, &g_srv_bloom_mips_y[i]), >= 0);
			}
		}

		const UINT x_mip0_width = g_swapchain_width >> 1;
		const UINT x_mip0_height = g_swapchain_height;

		// Create X MIPs and views.
		[[unlikely]] if (!g_rtv_bloom_mips_x[0]) {
			// Create X MIP0 and views.
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = x_mip0_width;
			tex_desc.Height = x_mip0_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[0]), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[0]), >= 0);

			// Create rest of X MIPs and views.
			for (UINT i = 1; i < g_bloom_nmips; ++i) {
				tex_desc.Width = std::max(1u, x_mip0_width >> i);
				tex_desc.Height = std::max(1u, x_mip0_height >> i);
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, &g_rtv_bloom_mips_x[i]), >= 0);
				ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, &g_srv_bloom_mips_x[i]), >= 0);
			}
		}

		//

		// Create CB.
		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		// Prefilter + downsample pass
		//

		// Create VS.
		[[unlikely]] if (!g_managed_resources.vertex_shaders["fullscreen_triangle"_h]) {
			create_vertex_shader(g_device, g_managed_resources.vertex_shaders["fullscreen_triangle"_h].put(), L"FullscreenTriangle_vs.hlsl");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_downsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_prefilter"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_prefilter"_h].put(), L"Bloom_impl.hlsl", "prefilter_ps");
		}

		D3D11_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(g_swapchain_width, g_swapchain_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[0], nullptr);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(g_managed_resources.vertex_shaders["fullscreen_triangle"_h].get(), nullptr, 0);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get(), nullptr, 0);
		const std::array bloom_prefilter_cbs = { g_managed_resources.buffers["CBLuminance"_h].get(), g_cb.get() };
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT - bloom_prefilter_cbs.size() + 1, bloom_prefilter_cbs.size(), bloom_prefilter_cbs.data());
		const std::array bloom_prefilter_srvs = { srv_scene.get(),  g_managed_resources.shader_resource_views["gLuminanceBufferSRV"_h].get() };
		ctx->PSSetShaderResources(0, bloom_prefilter_srvs.size(), bloom_prefilter_srvs.data());
		ctx->RSSetViewports(1, &viewport_x);
		ctx->OMSetBlendState(nullptr, nullptr, UINT_MAX);

		// Draw X pass.
		ctx->Draw(3, 0);

		std::vector<D3D11_VIEWPORT> viewports_y(g_bloom_nmips);
		viewports_y[0].Width = y_mip0_width;
		viewports_y[0].Height = y_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(x_mip0_width, x_mip0_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(0.0f, 1.0f);
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[0], nullptr);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_prefilter"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);
		ctx->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		ctx->Draw(3, 0);

		//

		// Downsample passes
		//

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get(), nullptr, 0);

		// Render downsample passes.
		for (UINT i = 1; i < g_bloom_nmips; ++i) {
			viewport_x.Width = std::max(1u, x_mip0_width >> i);
			viewport_x.Height = std::max(1u, x_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i - 1].Width, viewports_y[i - 1].Height);
			g_cb_data.axis = float2(1.0f, 0.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			g_cb_data.sigma = g_bloom_sigmas[i];
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i - 1]);
			ctx->RSSetViewports(1, &viewport_x);

			// Draw X pass.
			ctx->Draw(3, 0);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);
			ctx->RSSetViewports(1, &viewports_y[i]);

			// Draw Y pass.
			ctx->Draw(3, 0);
		}

		//

		// Upsample passes
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_upsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_upsample"_h].put(), L"Bloom_impl.hlsl", "upsample_ps");
		}

		// Create blend.
		[[unlikely]] if (!g_managed_resources.blends["bloom"_h]) {
			CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
			ensure(g_device->CreateBlendState(&desc, g_managed_resources.blends["bloom"_h].put()), >= 0);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_upsample"_h].get(), nullptr, 0);

		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// If both dst and src are D3D11_BLEND_BLEND_FACTOR,
			// factor of 0.5 will be enegrgy preserving.
			static constexpr FLOAT blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i - 1], nullptr);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i]);
			ctx->RSSetViewports(1, &viewports_y[i - 1]);
			ctx->OMSetBlendState(g_managed_resources.blends["bloom"_h].get(), blend_factor, UINT_MAX);

			ctx->Draw(3, 0);
		}

		//

		// BloomAdd pass
		//

		// Create blend.
		[[unlikely]] if (!g_managed_resources.blends["bloom_add"_h]) {
			CD3D11_BLEND_DESC desc(D3D11_DEFAULT);
			desc.RenderTarget[0].BlendEnable = TRUE;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			ensure(g_device->CreateBlendState(&desc, g_managed_resources.blends["bloom_add"_h].put()), >= 0);
		}

		D3D11_VIEWPORT viewport_add = {};
		viewport_add.Width = g_swapchain_width;
		viewport_add.Height = g_swapchain_height;

		FLOAT blend_factor[] = { g_bloom_intensity, g_bloom_intensity, g_bloom_intensity, 0.0f };

		// Bindings
		ctx->OMSetRenderTargets(1, &rtv_scene, nullptr);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[0]);
		ctx->RSSetViewports(1, &viewport_add);
		ctx->OMSetBlendState(g_managed_resources.blends["bloom_add"_h].get(), blend_factor, UINT_MAX);

		ctx->Draw(3, 0);

		//

		// Restore.
		ctx->IASetPrimitiveTopology(primitive_topology_original);
		ctx->VSSetShader(vs_original.get(), nullptr, 0);
		ctx->PSSetShader(ps_original.get(), nullptr, 0);
		ctx->PSSetConstantBuffers(13, 1, &cb_orginal);
		ctx->PSSetSamplers(0, 1, &ps_sampler_original);
		ctx->PSSetShaderResources(0, 1, &ps_srv_original);
		ctx->RSSetViewports(viewports_original.size(), viewports_original.data());
		ctx->RSSetState(rasterizer_original.get());
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0xAC274871.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0xAC274871.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0xAC274871"_h]) {
			const std::string srgb_str = g_trc == TRC_SRGB ? "SRGB" : "";
			const std::string gamma_str = std::to_string(g_gamma);
			const D3D_SHADER_MACRO defines[] = {
				{ srgb_str.c_str(), nullptr },
				{ "GAMMA", gamma_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0xAC274871"_h].put(), L"Tonemap_0xAC274871_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0xAC274871"_h].get(), nullptr, 0);

		return false;
	}

	return false;
}

static bool on_dispatch(reshade::api::command_list* cmd_list, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	#if 0
	return false;
	#endif

	auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
	Com_ptr<ID3D11ComputeShader> cs;
	ctx->CSGetShader(cs.put(), nullptr, nullptr);

	#if DEV
	Com_ptr<ID3D11Device> device;
	ctx->GetDevice(device.put());
	assert(device == g_device);
	#endif

	uint32_t hash;
	UINT size;
	HRESULT hr;

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_shadows_ao_denoiser_0x9F9DDC10.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_shadows_ao_denoiser_0x9F9DDC10.hash) {
		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["shadows_ao_denoiser_0x9F9DDC10"_h]) {
			create_compute_shader(g_device, g_managed_resources.compute_shaders["shadows_ao_denoiser_0x9F9DDC10"_h].put(), L"ShadowsAndAODenoise_0x9F9DDC10_cs.hlsl");
		}

		// Bindings.
		ctx->CSSetShader(g_managed_resources.compute_shaders["shadows_ao_denoiser_0x9F9DDC10"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_taa_0xD84C4AF0.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_taa_0xD84C4AF0.hash) {
		if (g_enable_dlss) {
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// Get UAV and it's resource.
			Com_ptr<ID3D11UnorderedAccessView> uav_output;
			ctx->CSGetUnorderedAccessViews(0, 1, uav_output.put());
			Com_ptr<ID3D11Resource> resource_output;
			uav_output->GetResource(resource_output.put());

			// UnpackMVs pass
			//

			// Create CS.
			[[unlikely]] if (!g_managed_resources.compute_shaders["unpack_mvs"_h]) {
				create_compute_shader(g_device, g_managed_resources.compute_shaders["unpack_mvs"_h].put(), L"UnpackMVs_cs.hlsl");
			}

			// Create UAV.
			[[unlikely]] if (!g_managed_resources.unordered_access_views["mvs"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["mvs"_h].put()), >= 0);
				ensure(g_device->CreateUnorderedAccessView(g_managed_resources.textures_2d["mvs"_h].get(), nullptr, g_managed_resources.unordered_access_views["mvs"_h].put()), >= 0);
			}

			// Bindings.
			ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["mvs"_h], nullptr);
			ctx->CSSetShader(g_managed_resources.compute_shaders["unpack_mvs"_h].get(), nullptr, 0);

			ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

			//

			// DLSS pass
			//

			Com_ptr<ID3D11ShaderResourceView> srv_scene;
			ctx->CSGetShaderResources(0, 1, srv_scene.put());
			Com_ptr<ID3D11Resource> resorce_scene;
			srv_scene->GetResource(resorce_scene.put());

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = resorce_scene.get();
			eval_params.Feature.pInOutput = resource_output.get();
			eval_params.pInDepth = g_managed_resources.resources["depth"_h].get();
			eval_params.pInMotionVectors = g_managed_resources.textures_2d["mvs"_h].get();

			// MVs are in NDC space so we need to scale them to screen space for DLSS.
			// Also for DLSS we need to flip the sign for x.
			eval_params.InMVScaleX = g_swapchain_width * -0.5;
			eval_params.InMVScaleY = g_swapchain_height * 0.5;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
			eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * -1.0f;
			eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * 1.0f;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			//

			return true;
		}
		return false;
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
				case g_ps_copy_0xBCA17DC2.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_copy_0xBCA17DC2.guid, sizeof(g_ps_copy_0xBCA17DC2.hash), &g_ps_copy_0xBCA17DC2.hash), >= 0);
					return;
				case g_ps_bloom_0xBA76C0BA.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xBA76C0BA.guid, sizeof(g_ps_bloom_0xBA76C0BA.hash), &g_ps_bloom_0xBA76C0BA.hash), >= 0);
					return;
				case g_ps_bloom_0xEF5ED883.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xEF5ED883.guid, sizeof(g_ps_bloom_0xEF5ED883.hash), &g_ps_bloom_0xEF5ED883.hash), >= 0);
					return;
				case g_ps_bloom_0xF45A211E.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0xF45A211E.guid, sizeof(g_ps_bloom_0xF45A211E.hash), &g_ps_bloom_0xF45A211E.hash), >= 0);
					return;
				case g_ps_bloom_add_0x77963D88.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_add_0x77963D88.guid, sizeof(g_ps_bloom_add_0x77963D88.hash), &g_ps_bloom_add_0x77963D88.hash), >= 0);
					return;
				case g_ps_bloom_threshold_0xC1DD61BF.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_threshold_0xC1DD61BF.guid, sizeof(g_ps_bloom_threshold_0xC1DD61BF.hash), &g_ps_bloom_threshold_0xC1DD61BF.hash), >= 0);
					return;
				case g_ps_tonemap_0xAC274871.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0xAC274871.guid, sizeof(g_ps_tonemap_0xAC274871.hash), &g_ps_tonemap_0xAC274871.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_shadows_ao_denoiser_0x9F9DDC10.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_shadows_ao_denoiser_0x9F9DDC10.guid, sizeof(g_cs_shadows_ao_denoiser_0x9F9DDC10.hash), &g_cs_shadows_ao_denoiser_0x9F9DDC10.hash), >= 0);
					return;
				case g_cs_taa_0xD84C4AF0.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_taa_0xD84C4AF0.guid, sizeof(g_cs_taa_0xD84C4AF0.hash), &g_cs_taa_0xD84C4AF0.hash), >= 0);
					return;
			}
		}
	}
}

static bool on_update_buffer_region(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size)
{
	auto native_resource = (ID3D11Resource*)resource.handle;
	Com_ptr<ID3D11Buffer> buffer;
	auto hr = native_resource->QueryInterface(buffer.put());
	if (SUCCEEDED(hr)) {
		D3D11_BUFFER_DESC desc;
		buffer->GetDesc(&desc);

		// This alone should be reliable? Needs testing!
		if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 1072) {

			g_jitter_x = ((CBViewProjection*)data)->fProj.m20;
			g_jitter_y = ((CBViewProjection*)data)->fProj.m21;
		}
	}
	return false;
}

static bool on_create_resource_view(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc)
{
	auto resource_desc = device->get_resource_desc(resource);

	// Try to filter only render targets that we have upgraded.
	if ((resource_desc.usage & reshade::api::resource_usage::render_target) != 0 || (resource_desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (resource_desc.texture.format == reshade::api::format::r16g16b16a16_float) {
			desc.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}

		if (resource_desc.texture.format == reshade::api::format::r32_float) {
			desc.format = reshade::api::format::r32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}
		if (desc.texture.format == reshade::api::format::r16_float) {
			desc.texture.format = reshade::api::format::r32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
		// Even on highest setting the game doesn't consitently use 16x.
		desc.max_anisotropy = 16.0f;

		// As recommended for DLAA.
		desc.mip_lod_bias += -1.0f;

		return true;
	}

	return false;
}

// Prevent entering fullscreen mode.
static bool on_set_fullscreen_state(reshade::api::swapchain* swapchain, bool fullscreen, void* hmonitor)
{
	if (g_force_modern_windowed && fullscreen) {
		return true;
	}
	return false;
}

static bool on_create_swapchain(reshade::api::device_api api, reshade::api::swapchain_desc& desc, void* hwnd)
{
	#if 0
	return false;
	#endif

	if (g_force_modern_windowed) {
		//desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
		desc.back_buffer_count = std::max(2u, desc.back_buffer_count);
		desc.present_mode = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.fullscreen_state = false;
	}

	if (g_force_vsync_off) {
		if (g_force_modern_windowed) {
			desc.present_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
		desc.fullscreen_refresh_rate = 0.0f;
		desc.sync_interval = 0;
	}

	return true;
}

static void on_init_swapchain(reshade::api::swapchain* swapchain, bool resize)
{
	auto native_swapchain = (IDXGISwapChain*)swapchain->get_native();
	DXGI_SWAP_CHAIN_DESC desc;
	native_swapchain->GetDesc(&desc);

	// Hook IDXGISwapChain::Present
	if (!g_original_present && g_force_vsync_off && g_force_modern_windowed) {
		auto vtable = *(void***)native_swapchain;
		ensure(MH_CreateHook(vtable[8], &detour_present, (void**)&g_original_present), == MH_OK);
		ensure(MH_EnableHook(vtable[8]), == MH_OK);
	}

	// Save device.
	ensure(native_swapchain->GetDevice(IID_PPV_ARGS(&g_device)), >= 0);

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	// Set maximum frame latency to 1.
	Com_ptr<IDXGIDevice1> dxgi_device;
	auto hr = g_device->QueryInterface(dxgi_device.put());
	if (SUCCEEDED(hr)) {
		ensure(dxgi_device->SetMaximumFrameLatency(1), >= 0);
	}

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		if (!resize) {
			DLSS::get_instance().init(g_device);
		}
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
	}
}

static void on_destroy_device(reshade::api::device* device)
{
	if (device->get_native() != (uintptr_t)g_device) {
		return;
	}
	if (g_enable_dlss) {
		DLSS::get_instance().shutdown();
	}
	g_cb.reset();
	g_managed_resources.clear();

	release_com_array(g_rtv_bloom_mips_y);
	release_com_array(g_srv_bloom_mips_y);
	release_com_array(g_rtv_bloom_mips_x);
	release_com_array(g_srv_bloom_mips_x);
}

static void read_config()
{
	if (!reshade::get_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss)) {
		reshade::set_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss);
	}

	if (!reshade::get_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset)) {
		reshade::set_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset);
	}
	switch (g_user_set_dlss_preset) {
			case 0: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default; break;
			case 1: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_E; break;
			case 2: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F; break;
			case 3: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_K; break;
			case 4: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_L; break;
			case 5: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_M; break;
			default: assert(false);
	}

	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	if (!reshade::get_config_value(nullptr, NAME, "TRC", g_trc)) {
		reshade::set_config_value(nullptr, NAME, "TRC", g_trc);
	}
	if (!reshade::get_config_value(nullptr, NAME, "Gamma", g_gamma)) {
		reshade::set_config_value(nullptr, NAME, "Gamma", g_gamma);
	}
	if (!reshade::get_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
	}
	if (!reshade::get_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
	#if DEV
	if (ImGui::Button("Dev button")) {
	}
	ImGui::Spacing();

	// The game may set this a bit later.
	if (ImGui::Button("Check MaximumFrameLatency")) {
		Com_ptr<IDXGIDevice1> dxgi_device;
		ensure(g_device->QueryInterface(dxgi_device.put()), >= 0);
		UINT max_latency;
		ensure(dxgi_device->GetMaximumFrameLatency(&max_latency), >= 0);
		log_debug("MaximumFrameLatency: {}", max_latency);
	}
	ImGui::Spacing();

	if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1, 10)) {
		release_com_array(g_rtv_bloom_mips_y);
		release_com_array(g_srv_bloom_mips_y);
		release_com_array(g_rtv_bloom_mips_x);
		release_com_array(g_srv_bloom_mips_x);
		g_rtv_bloom_mips_y.resize(g_bloom_nmips);
		g_srv_bloom_mips_y.resize(g_bloom_nmips);
		g_rtv_bloom_mips_x.resize(g_bloom_nmips);
		g_srv_bloom_mips_x.resize(g_bloom_nmips);
		g_bloom_sigmas.resize(g_bloom_nmips);
	}
	for (int i = 0; i < g_bloom_nmips; ++i) {
		const std::string name = "Sigma" + std::to_string(i);
		ImGui::SliderFloat(name.c_str(), &g_bloom_sigmas[i], 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	}
	ImGui::NewLine();
	#endif

	if (ImGui::Checkbox("Enable DLSS (DLAA)", &g_enable_dlss)) {
		if (g_enable_dlss) {
			Com_ptr<ID3D11DeviceContext> ctx;
			g_device->GetImmediateContext(ctx.put());
			DLSS::get_instance().init(g_device);
			DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
		}
		else {
			DLSS::get_instance().shutdown();
		}
		reshade::set_config_value(nullptr, NAME, "EnableDLSS", g_enable_dlss);
	}
	ImGui::BeginDisabled(!g_enable_dlss);
	static constexpr std::array dlss_preset_items = { "Default", "E", "F", "K", "L", "M" };
	if (ImGui::Combo("DLSS preset", &g_user_set_dlss_preset, dlss_preset_items.data(), dlss_preset_items.size())) {
		switch (g_user_set_dlss_preset) {
			case 0: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default; break;
			case 1: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_E; break;
			case 2: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F; break;
			case 3: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_K; break;
			case 4: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_L; break;
			case 5: g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_M; break;
			default: assert(false);
		}
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
		reshade::set_config_value(nullptr, NAME, "DLSSPreset", g_user_set_dlss_preset);
	}
	if (g_enable_dlss) {
		if (g_dlss_status) {
			ImGui::Text("DLSS status: OK.");
		}
		else {
			ImGui::Text("DLSS status: Faild or not running!");
		}
		g_dlss_status = false;
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
	}
	ImGui::Spacing();

	static constexpr std::array trc_items = { "sRGB", "Gamma" };
	if (ImGui::Combo("TRC", &g_trc, trc_items.data(), trc_items.size())) {
		g_managed_resources.pixel_shaders["tonemap_0xAC274871"_h].reset();
		reshade::set_config_value(nullptr, NAME, "TRC", g_trc);
	}
	ImGui::BeginDisabled(g_trc == TRC_SRGB);
	if (ImGui::SliderFloat("Gamma", &g_gamma, 1.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["tonemap_0xAC274871"_h].reset();
		reshade::set_config_value(nullptr, NAME, "Gamma", g_gamma);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	if (ImGui::Checkbox("Force modern windowed", &g_force_modern_windowed)) {
		reshade::set_config_value(nullptr, NAME, "ForceModernWindowed", g_force_modern_windowed);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Forces modern borderless or non borderless windowed mod.\nRequires restart.");
	}
	ImGui::Spacing();

	if (ImGui::Checkbox("Force vsync off", &g_force_vsync_off)) {
		reshade::set_config_value(nullptr, NAME, "ForceVsyncOff", g_force_vsync_off);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetItemTooltip("Requires restart.");
	}
	ImGui::Spacing();
}

// The game is throwing EXCEPTION_BREAKPOINT which will crash the game if debugger isn't attached (even in release build!).
static LONG CALLBACK exception_breakpoint_handler(EXCEPTION_POINTERS* ep)
{
	if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
		ep->ContextRecord->Rip += 1;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule) || MH_Initialize() != MH_OK) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			AddVectoredExceptionHandler(1, exception_breakpoint_handler);

			init_graphical_upgrade_path();
			read_config();

			// Bloom.
			g_bloom_nmips = 6;
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 1.5f;
			g_bloom_sigmas[1] = 2.0f;
			g_bloom_sigmas[2] = 2.0f;
			g_bloom_sigmas[3] = 2.0f;
			g_bloom_sigmas[4] = 1.0f;
			g_bloom_sigmas[5] = 1.0f;

			reshade::register_event<reshade::addon_event::draw>(on_draw);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::update_buffer_region>(on_update_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource_view>(on_create_resource_view);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			RemoveVectoredExceptionHandler(exception_breakpoint_handler);
			MH_Uninitialize();
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
