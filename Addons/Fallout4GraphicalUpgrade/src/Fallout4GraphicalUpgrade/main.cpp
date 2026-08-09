#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "Fallout4GraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v3.0.1";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/Fallout4GraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_cs_linearize_and_downsample_depth_0x1D1E3148 = { 0x1D1E3148, { 0x394e69fa, 0x8ebb, 0x4101, { 0xa2, 0x88, 0x7d, 0x29, 0x35, 0x52, 0x2, 0x22 }}};
constexpr Shader_hash g_cs_ssao_main_0x0307C239 = { 0x0307C239, { 0x9d62deb7, 0x6262, 0x4e52, { 0xa4, 0xc0, 0xdb, 0xa0, 0x58, 0x8a, 0xec, 0x95 }}};
constexpr Shader_hash g_cs_ssao_denoise_x_0xE151AD86 = { 0xE151AD86, { 0x382abde7, 0xaefb, 0x4f71, { 0x86, 0xc2, 0x57, 0xf9, 0xcd, 0xbf, 0xe6, 0x62 }}};
constexpr Shader_hash g_cs_ssao_denoise_y_0x7E8F370A = { 0x7E8F370A, { 0x59e5c9bc, 0x20d7, 0x4c4a, { 0x89, 0x72, 0x86, 0x72, 0x4a, 0x38, 0x9b, 0x20 }}};
constexpr Shader_hash g_ps_downsample_0x05868F11 = { 0x05868F11, { 0x942e66e7, 0x8906, 0x4fbe, { 0xb3, 0x69, 0x49, 0x85, 0x99, 0x7f, 0xa7, 0x93 }}};
constexpr Shader_hash g_ps_bloom_0x5D1B5B1A = { 0x5D1B5B1A, { 0x5fb9020c, 0x18a4, 0x43a0, { 0x90, 0x6e, 0xbb, 0xcd, 0xc5, 0xa7, 0x89, 0x74 }}};
constexpr Shader_hash g_ps_apply_blur_kernel_0x9B7CD304 = { 0x9B7CD304, { 0x2f52ab9c, 0x54a6, 0x439d, { 0xa3, 0xc1, 0xf9, 0x9, 0x6f, 0xc9, 0xeb, 0x20 }}};
constexpr Shader_hash g_ps_tonemap_0x80802E60 = { 0x80802E60, { 0x3e347b1a, 0x7279, 0x4b58, { 0xa6, 0x77, 0xc3, 0xb4, 0x56, 0x5a, 0x73, 0x7 }}};
constexpr Shader_hash g_ps_taa_0x61CC29E6 = { 0x61CC29E6, { 0x6671e3c6, 0xad72, 0x453f, { 0xa1, 0xbb, 0x6b, 0x3a, 0x3c, 0x58, 0x56, 0x48 }}};

//

static ID3D11Device* g_device;
static Managed_resources g_managed_resources;
static Graphical_upgrade_cb_data g_cb_data;
static Com_ptr<ID3D11Buffer> g_cb;
static int g_swapchain_width;
static int g_swapchain_height;
uintptr_t g_mapped_cb_handle;
void* g_mapped_cb_data;
static bool g_force_vsync_off = true;
static bool g_force_modern_windowed = true;
static float g_amd_ffx_cas_sharpness = 0.0f;

// GTAO
constexpr size_t GTAO_DEPTH_MIP_LEVELS = 5;
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra
static bool g_has_drawn_ssao;

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static int g_bloom_input_width;
static int g_bloom_input_height;
static bool g_has_run_ps_bloom_0x5D1B5B1A;

// DLSS
constexpr int g_dlss_flags{
	NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
};
static NVSDK_NGX_DLSS_Hint_Render_Preset g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F;
static int g_user_set_dlss_preset;
static bool g_enable_dlss;
static bool g_dlss_status;
static float g_jitter_x;
static float g_jitter_y;

// Device resources.
static std::array<ID3D11UnorderedAccessView*, GTAO_DEPTH_MIP_LEVELS> g_uav_gtao_prefilter_depths16x16;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_y;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_y;
static std::vector<ID3D11RenderTargetView*> g_rtv_bloom_mips_x;
static std::vector<ID3D11ShaderResourceView*> g_srv_bloom_mips_x;

static bool on_draw_indexed(reshade::api::command_list* cmd_list, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
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
	hr = ps->GetPrivateData(g_ps_downsample_0x05868F11.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_downsample_0x05868F11.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["downsample_0x05868F11"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["downsample_0x05868F11"_h].put(), L"Downsample_0x05868F11_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["downsample_0x05868F11"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x5D1B5B1A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x5D1B5B1A.hash) {
		g_has_run_ps_bloom_0x5D1B5B1A = true;

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_0x5D1B5B1A"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_0x5D1B5B1A"_h].put(), L"Bloom_0x5D1B5B1A_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_0x5D1B5B1A"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_apply_blur_kernel_0x9B7CD304.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_apply_blur_kernel_0x9B7CD304.hash) {
		if (!g_has_run_ps_bloom_0x5D1B5B1A) {
			return false;
		}
		g_has_run_ps_bloom_0x5D1B5B1A = false;

		// Backup viewport.
		D3D11_VIEWPORT viewport_original;
		UINT nviewports = 1;
		ctx->RSGetViewports(&nviewports, &viewport_original);

		// Backup sampler.
		Com_ptr<ID3D11SamplerState> smp_original;
		ctx->PSGetSamplers(0, 1, smp_original.put());

		// Backup Blend.
		Com_ptr<ID3D11BlendState> blend_original;
		FLOAT blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Backup RTV and DSV.
		Com_ptr<ID3D11RenderTargetView> rtv_original;
		Com_ptr<ID3D11DepthStencilView> dsv;
		ctx->OMGetRenderTargets(1, rtv_original.put(), dsv.put());

		// Get bloom input width and height.
		[[unlikely]] if (!g_bloom_input_width) {
			// Get SRV0's texture description.
			Com_ptr<ID3D11ShaderResourceView> srv;
			ctx->PSGetShaderResources(0, 1, srv.put());
			Com_ptr<ID3D11Resource> resource;
			srv->GetResource(resource.put());
			Com_ptr<ID3D11Texture2D> tex;
			ensure(resource->QueryInterface(tex.put()), >= 0);
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);

			g_bloom_input_width = tex_desc.Width;
			g_bloom_input_height = tex_desc.Height;
		}

		// Create MIPs and views.
		//

		const UINT x_mip0_width = g_bloom_input_width >> 1;
		const UINT x_mip0_height = g_bloom_input_height;

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

		const UINT y_mip0_width = g_bloom_input_width >> 1;
		const UINT y_mip0_height = g_bloom_input_height >> 1;

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

		//

		// The first downsample pass
		//
		// The input is prefiltered.
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_downsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create CB.
		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		// Create sampler.
		[[unlikely]] if (!g_managed_resources.samplers["linear"_h]) {
			create_sampler_linear_clamp(g_device, g_managed_resources.samplers["linear"_h].put());
		}

		D3D11_VIEWPORT viewport_x = {};
		viewport_x.Width = x_mip0_width;
		viewport_x.Height = x_mip0_height;

		// Update CB.
		g_cb_data.src_size = float2(g_bloom_input_width, g_bloom_input_height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		g_cb_data.axis = float2(1.0f, 0.0f);
		g_cb_data.sigma = g_bloom_sigmas[0];
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_x[0], nullptr);
		ctx->RSSetViewports(1, &viewport_x);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_downsample"_h].get(), nullptr, 0);
		ctx->PSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->PSSetSamplers(0, 1, &g_managed_resources.samplers["linear"_h]);

		// Draw X pass.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

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
		ctx->RSSetViewports(1, &viewports_y[0]);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);

		// Draw Y pass.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		// Rest of downsample passes
		//

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
			ctx->RSSetViewports(1, &viewport_x);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i - 1]);

			// Draw X pass.
			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

			viewports_y[i].Width = std::max(1u, y_mip0_width >> i);
			viewports_y[i].Height = std::max(1u, y_mip0_height >> i);

			// Update CB.
			g_cb_data.src_size = float2(viewport_x.Width, viewport_x.Height);
			g_cb_data.axis = float2(0.0f, 1.0f);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i], nullptr);
			ctx->RSSetViewports(1, &viewports_y[i]);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);

			// Draw Y pass.
			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
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

		// If both dst and src are D3D11_BLEND_BLEND_FACTOR,
		// factor of 0.5 will be enegrgy preserving.
		static constexpr FLOAT blend_factor[] = { 0.5f, 0.5f, 0.5f, 0.5f };

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_upsample"_h].get(), nullptr, 0);
		ctx->OMSetBlendState(g_managed_resources.blends["bloom"_h].get(), blend_factor, UINT_MAX);

		for (int i = g_bloom_nmips - 1; i > 0; --i) {
			// Update CB.
			g_cb_data.src_size = float2(viewports_y[i].Width, viewports_y[i].Height);
			g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
			update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_rtv_bloom_mips_y[i - 1], nullptr);
			ctx->RSSetViewports(1, &viewports_y[i - 1]);
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[i]);

			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
		}

		// The final upsample pass
		//
		// We are binding the original RTV.
		//

		// Update CB.
		g_cb_data.src_size = float2(viewports_y[0].Width, viewports_y[0].Height);
		g_cb_data.inv_src_size = float2(1.0f / g_cb_data.src_size.x, 1.0f / g_cb_data.src_size.y);
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Bindings.
		ctx->OMSetRenderTargets(1, &rtv_original, dsv.get());
		ctx->RSSetViewports(1, &viewport_original);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[0]);

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		// Restore.
		// May not be necessary, needs testing.
		ctx->PSSetSamplers(0, 1, &smp_original);
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x80802E60.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x80802E60.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x80802E60"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0x80802E60"_h].put(), L"Tonemap_0x80802E60_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x80802E60"_h].get(), nullptr, 0);
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_taa_0x61CC29E6.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_taa_0x61CC29E6.hash) {
		if (g_enable_dlss) {
			// Get the TAA CB. We need to track it later on map/unmap.
			ctx->PSGetConstantBuffers(2, 1, g_managed_resources.buffers["taa_0x61CC29E6_cb2"_h].put());

			// DLSS pass
			//

			// DLSS requires an immediate context for execution!
			assert(ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

			// Get SRVs.
			std::array<ID3D11ShaderResourceView*, 4> srvs;
			ctx->PSGetShaderResources(0, srvs.size(), srvs.data());

			// Get resources from SRVs.
			Com_ptr<ID3D11Resource> resource_scene;
			srvs[0]->GetResource(resource_scene.put());
			Com_ptr<ID3D11Resource> resource_mvs;
			srvs[2]->GetResource(resource_mvs.put());
			Com_ptr<ID3D11Resource> resource_depth;
			srvs[3]->GetResource(resource_depth.put());

			std::array<ID3D11ShaderResourceView*, 4> srvs_null = {};
			ctx->PSSetShaderResources(0, 4, srvs_null.data());

			// Get RTVs.
			std::array<ID3D11RenderTargetView*, 2> rtvs;
			ctx->OMGetRenderTargets(rtvs.size(), rtvs.data(), nullptr);

			// Create the output resource for DLSS.
			[[unlikely]] if (!g_managed_resources.textures_2d["dlss_output"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
				ensure(g_device->CreateShaderResourceView(g_managed_resources.textures_2d["dlss_output"_h].get(), nullptr, g_managed_resources.shader_resource_views["dlss_output"_h].put()), >= 0);
			}

			#if DEV && SHOW_AO
			g_managed_resources.unordered_access_views["gtao_denoise_pass2"_h]->GetResource(resource_scene.put());
			#endif

			NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
			eval_params.Feature.pInColor = resource_scene.get();
			eval_params.Feature.pInOutput = g_managed_resources.textures_2d["dlss_output"_h].get();
			eval_params.pInDepth = resource_depth.get();
			eval_params.pInMotionVectors = resource_mvs.get();

			// MVs are in UV space so we need to scale them to screen space for DLSS.
			eval_params.InMVScaleX = g_swapchain_width;
			eval_params.InMVScaleY = g_swapchain_height;

			eval_params.InRenderSubrectDimensions.Width = g_swapchain_width;
			eval_params.InRenderSubrectDimensions.Height = g_swapchain_height;

			// Jitters are in UV offsets so we need to scale them to pixel offsets for DLSS.
			eval_params.InJitterOffsetX = g_jitter_x * (float)g_swapchain_width * 1.0f;
			eval_params.InJitterOffsetY = g_jitter_y * (float)g_swapchain_height * -1.0f;

			g_dlss_status = DLSS::get_instance().draw(ctx, eval_params);

			//

			// Linearize pass
			//

			// Create PS.
			[[unlikely]] if (!g_managed_resources.pixel_shaders["linearize"_h]) {
				create_pixel_shader(g_device, g_managed_resources.pixel_shaders["linearize"_h].put(), L"Linearize_ps.hlsl");
			}

			// Create RT.
			[[unlikely]] if (!g_managed_resources.render_target_views["linearize"_h]) {
				D3D11_TEXTURE2D_DESC tex_desc = {};
				tex_desc.Width = g_swapchain_width;
				tex_desc.Height = g_swapchain_height;
				tex_desc.MipLevels = 1;
				tex_desc.ArraySize = 1;
				tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
				tex_desc.SampleDesc.Count = 1;
				tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				Com_ptr<ID3D11Texture2D> tex;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
				ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["linearize"_h].put()), >= 0);
				ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["linearize"_h].put()), >= 0);
			}

			// Bindings.
			ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["linearize"_h], nullptr);
			ctx->PSSetShader(g_managed_resources.pixel_shaders["linearize"_h].get(), nullptr, 0);
			ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["dlss_output"_h]);

			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

			//

			// AMD FFX CAS pass
			//

			// Create PS.
			[[unlikely]] if (!g_managed_resources.pixel_shaders["amd_ffx_cas"_h]) {
				const std::string amd_ffx_cas_sharpness_str = std::to_string(g_amd_ffx_cas_sharpness);
				const D3D_SHADER_MACRO defines[] = {
					{ "SHARPNESS", amd_ffx_cas_sharpness_str.c_str() },
					{ nullptr, nullptr }
				};
				create_pixel_shader(g_device, g_managed_resources.pixel_shaders["amd_ffx_cas"_h].put(), L"AMD_FFX_CAS_ps.hlsl", "main", defines);
			}

			// Bindings.
			ctx->OMSetRenderTargets(1, &rtvs[1], nullptr);
			ctx->PSSetShader(g_managed_resources.pixel_shaders["amd_ffx_cas"_h].get(), nullptr, 0);
			ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["linearize"_h]);

			cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

			//

			release_com_array(srvs);
			release_com_array(rtvs);

			return true;
		}
		return false;
	}

	if (g_has_drawn_ssao) {
		// This should be reliable.
		// Replace the original SSAO SRV with the GTAO SRV.
		// The original SSAO should be bound only onece as SRV9.
		// Confirmed PS permuatations: 0xEDF0538E, 0xC3B3F9E6, 0xBDFB307C
		Com_ptr<ID3D11ShaderResourceView> srv;
		ctx->PSGetShaderResources(9, 1, srv.put());
		if (srv) {
			Com_ptr<ID3D11Resource> resource;
			srv->GetResource(resource.put());
			if (resource == g_managed_resources.resources["ssao"_h]) {
				ctx->PSSetShaderResources(9, 1, &g_managed_resources.shader_resource_views["gtao_denoise_pass2"_h]);
				g_has_drawn_ssao = false;
			}
		}
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
	hr = cs->GetPrivateData(g_cs_linearize_and_downsample_depth_0x1D1E3148.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_linearize_and_downsample_depth_0x1D1E3148.hash) {
		// The game is using these depths elsewhere so we can't just skip the original draw.
		cmd_list->dispatch(group_count_x, group_count_y, group_count_z);

		// XeGTAOPrefilterDepths16x16 pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].put(), L"GTAO_impl.hlsl", "prefilter_depths16x16_cs", defines);
		}

		// Create point clamp sampler.
		[[unlikely]] if (!g_managed_resources.samplers["point_clamp"_h]) {
			create_sampler_point_clamp(g_device, g_managed_resources.samplers["point_clamp"_h].put());
		}

		// Create prefilter depths views.
		[[unlikely]] if (!g_uav_gtao_prefilter_depths16x16[0]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = GTAO_DEPTH_MIP_LEVELS;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);

			// Create UAVs for each MIP.
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = tex_desc.Format;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			for (int i = 0; i < g_uav_gtao_prefilter_depths16x16.size(); ++i) {
			   uav_desc.Texture2D.MipSlice = i;
			   ensure(g_device->CreateUnorderedAccessView(tex.get(), &uav_desc, &g_uav_gtao_prefilter_depths16x16[i]), >= 0);
			}

			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_prefilter_depths16x16"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, g_uav_gtao_prefilter_depths16x16.size(), g_uav_gtao_prefilter_depths16x16.data(), nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].get(), nullptr, 0);
		ctx->CSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, GTAO_DEPTH_MIP_LEVELS> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//

		return true;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ssao_main_0x0307C239.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ssao_main_0x0307C239.hash) {
		g_has_drawn_ssao = true;

		// XeGTAOMain pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["gtao_main_pass"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const std::string gtao_quality_val = std::to_string(g_gtao_quality);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ "GTAO_QUALITY", gtao_quality_val.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["gtao_main_pass"_h].put(), L"GTAO_impl.hlsl", "main_pass_cs", defines);
		}

		// Create CB.
		[[unlikely]] if (!g_cb) {
			create_constant_buffer(g_device, sizeof(g_cb_data), g_cb.put());
		}

		++g_cb_data.frame_index; // It's only used by GTAO so this should be fine.
		update_constant_buffer(ctx, g_cb.get(), &g_cb_data, sizeof(g_cb_data));

		// Create AO term and Edges views.
		[[unlikely]] if (!g_managed_resources.unordered_access_views["gtao_main_pass"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateUnorderedAccessView(tex.get(), nullptr, g_managed_resources.unordered_access_views["gtao_main_pass"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_main_pass"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["gtao_main_pass"_h], nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_main_pass"_h].get(), nullptr, 0);
		ctx->CSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->CSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_prefilter_depths16x16"_h]);

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

		//

		// Doing 2 XeGTAODenoisePass passes correspond to "Denoising level: Medium" from the XeGTAO demo.

		// XeGTAODenoisePass1 pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["gtao_denoise_pass1"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["gtao_denoise_pass1"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_cs", defines);
		}

		// Create AO term and Edges views.
		[[unlikely]] if (!g_managed_resources.unordered_access_views["gtao_denoise_pass1"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateUnorderedAccessView(tex.get(), nullptr, g_managed_resources.unordered_access_views["gtao_denoise_pass1"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_denoise_pass1"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["gtao_denoise_pass1"_h], nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_denoise_pass1"_h].get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_main_pass"_h]);

		ctx->Dispatch((g_swapchain_width + 8 * 2 - 1) / (8 * 2), (g_swapchain_height + 8 - 1) / 8,1);

		//

		// XeGTAODenoisePass2 pass
		//

		// Create CS.
		[[unlikely]] if (!g_managed_resources.compute_shaders["gtao_denoise_pass2"_h]) {
			const std::string viewport_pixel_size_str = std::format("float2({},{})", 1.0f / (float)g_swapchain_width, 1.0f / (float)g_swapchain_height);
			const D3D_SHADER_MACRO defines[] = {
				{ "XE_GTAO_FINAL_APPLY", "1" },
				{ "VIEWPORT_PIXEL_SIZE", viewport_pixel_size_str.c_str() },
				{ nullptr, nullptr }
			};
			create_compute_shader(g_device, g_managed_resources.compute_shaders["gtao_denoise_pass2"_h].put(), L"GTAO_impl.hlsl", "denoise_pass_cs", defines);
		}

		// Create AO term and Edges views.
		[[unlikely]] if (!g_managed_resources.unordered_access_views["gtao_denoise_pass2"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // The original shader (the last SSAO pass) draws to rgba8_unorm, but only r is later used?
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateUnorderedAccessView(tex.get(), nullptr, g_managed_resources.unordered_access_views["gtao_denoise_pass2"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["gtao_denoise_pass2"_h].put()), >= 0);
		}

		// Bindings.
		ctx->CSSetUnorderedAccessViews(0, 1, &g_managed_resources.unordered_access_views["gtao_denoise_pass2"_h], nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_denoise_pass2"_h].get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_denoise_pass1"_h]);

		ctx->Dispatch((g_swapchain_width + 8 * 2 - 1) / (8 * 2), (g_swapchain_height + 8 - 1) / 8, 1);

		//
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ssao_denoise_x_0xE151AD86.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ssao_denoise_x_0xE151AD86.hash) {
		if (g_has_drawn_ssao) {
			return true;
		}
		return false;
	}

	size = sizeof(hash);
	hr = cs->GetPrivateData(g_cs_ssao_denoise_y_0x7E8F370A.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_cs_ssao_denoise_y_0x7E8F370A.hash) {
		if (g_has_drawn_ssao) {
			// We need to track the SSAO resource later.
			Com_ptr<ID3D11UnorderedAccessView> uav;
			ctx->CSGetUnorderedAccessViews(0, 1, uav.put());
			uav->GetResource(g_managed_resources.resources["ssao"_h].put());
		
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
				case g_ps_downsample_0x05868F11.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_downsample_0x05868F11.guid, sizeof(g_ps_downsample_0x05868F11.hash), &g_ps_downsample_0x05868F11.hash), >= 0);
					return;
				case g_ps_bloom_0x5D1B5B1A.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x5D1B5B1A.guid, sizeof(g_ps_bloom_0x5D1B5B1A.hash), &g_ps_bloom_0x5D1B5B1A.hash), >= 0);
					return;
				case g_ps_apply_blur_kernel_0x9B7CD304.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_apply_blur_kernel_0x9B7CD304.guid, sizeof(g_ps_apply_blur_kernel_0x9B7CD304.hash), &g_ps_apply_blur_kernel_0x9B7CD304.hash), >= 0);
					return;
				case g_ps_tonemap_0x80802E60.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x80802E60.guid, sizeof(g_ps_tonemap_0x80802E60.hash), &g_ps_tonemap_0x80802E60.hash), >= 0);
					return;
				case g_ps_taa_0x61CC29E6.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_taa_0x61CC29E6.guid, sizeof(g_ps_taa_0x61CC29E6.hash), &g_ps_taa_0x61CC29E6.hash), >= 0);
					return;
			}
		}
		if (subobjects[i].type == reshade::api::pipeline_subobject_type::compute_shader) {
			auto desc = (reshade::api::shader_desc*)subobjects[i].data;
			const auto hash = compute_crc32((const uint8_t*)desc->code, desc->code_size);
			switch (hash) {
				case g_cs_linearize_and_downsample_depth_0x1D1E3148.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_linearize_and_downsample_depth_0x1D1E3148.guid, sizeof(g_cs_linearize_and_downsample_depth_0x1D1E3148.hash), &g_cs_linearize_and_downsample_depth_0x1D1E3148.hash), >= 0);
					return;
				case g_cs_ssao_main_0x0307C239.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ssao_main_0x0307C239.guid, sizeof(g_cs_ssao_main_0x0307C239.hash), &g_cs_ssao_main_0x0307C239.hash), >= 0);
					return;
				case g_cs_ssao_denoise_x_0xE151AD86.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ssao_denoise_x_0xE151AD86.guid, sizeof(g_cs_ssao_denoise_x_0xE151AD86.hash), &g_cs_ssao_denoise_x_0xE151AD86.hash), >= 0);
					return;
				case g_cs_ssao_denoise_y_0x7E8F370A.hash:
					ensure(((ID3D11ComputeShader*)pipeline.handle)->SetPrivateData(g_cs_ssao_denoise_y_0x7E8F370A.guid, sizeof(g_cs_ssao_denoise_y_0x7E8F370A.hash), &g_cs_ssao_denoise_y_0x7E8F370A.hash), >= 0);
					return;
			}
		}
	}
}

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;

	// This should be reliable? Needs testing!
	if (buffer == g_managed_resources.buffers["taa_0x61CC29E6_cb2"_h]) {
		g_mapped_cb_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	if (buffer == g_managed_resources.buffers["taa_0x61CC29E6_cb2"_h]) {
		auto data = (float4*)g_mapped_cb_data;

		// Should be 8 long Halton(2,3) sequence in UV offsets.
		g_jitter_x = data[1].x;
		g_jitter_y = data[1].y;
	}
}

static bool on_create_resource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state)
{
	// Filter RTs and UAVs.
	if ((desc.usage & reshade::api::resource_usage::render_target) != 0 || (desc.usage & reshade::api::resource_usage::unordered_access) != 0) {
		if (desc.texture.format == reshade::api::format::r11g11b10_float) {
			desc.texture.format = reshade::api::format::r16g16b16a16_float;
			return true;
		}

		if (desc.texture.format == reshade::api::format::r16g16_float) {
			desc.texture.format = reshade::api::format::r32g32_float;
			return true;
		}
	}

	return false;
}

static bool on_create_sampler(reshade::api::device* device, reshade::api::sampler_desc& desc)
{
	if (desc.filter == reshade::api::filter_mode::anisotropic) {
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
		desc.back_buffer.texture.format = reshade::api::format::r10g10b10a2_unorm;
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

	// Save device.
	g_device = (ID3D11Device*)swapchain->get_device()->get_native();

	// Save swapchain size.
	g_swapchain_width = desc.BufferDesc.Width;
	g_swapchain_height = desc.BufferDesc.Height;

	if (g_enable_dlss) {
		Com_ptr<ID3D11DeviceContext> ctx;
		g_device->GetImmediateContext(ctx.put());
		if (!resize) {
			DLSS::get_instance().init(g_device);
		}
		DLSS::get_instance().create_feature(ctx.get(), g_swapchain_width, g_swapchain_height, g_dlss_preset, g_dlss_flags);
	}

	// Reset reolution dependent resources.
	//

	g_managed_resources.textures_2d["dlss_output"_h].reset();
	g_managed_resources.render_target_views["linearize"_h].reset();

	// GTAO.
	g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].reset();
	reset_com_array(g_uav_gtao_prefilter_depths16x16);
	g_managed_resources.compute_shaders["gtao_main_pass"_h].reset();
	g_managed_resources.unordered_access_views["gtao_main_pass"_h].reset();
	g_managed_resources.compute_shaders["gtao_denoise_pass1"_h].reset();
	g_managed_resources.unordered_access_views["gtao_denoise_pass1"_h].reset();
	g_managed_resources.unordered_access_views["gtao_denoise_pass2"_h].reset();
	g_managed_resources.compute_shaders["gtao_denoise_pass2"_h].reset();

	// Bloom.
	reset_com_array(g_rtv_bloom_mips_y);
	reset_com_array(g_srv_bloom_mips_y);
	reset_com_array(g_rtv_bloom_mips_x);
	reset_com_array(g_srv_bloom_mips_x);
	g_bloom_input_width = 0;
	g_bloom_input_height = 0;

	//
}

static void on_init_device(reshade::api::device* device)
{
	#if 0
	return;
	#endif

	// Set maximum frame latency to 1.
	auto native_device = (ID3D11Device*)device->get_native();
	Com_ptr<IDXGIDevice1> dxgi_device;
	auto hr = native_device->QueryInterface(dxgi_device.put());
	if (SUCCEEDED(hr)) {
		ensure(dxgi_device->SetMaximumFrameLatency(1), >= 0);
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

	reset_com_array(g_uav_gtao_prefilter_depths16x16);
	reset_com_array(g_rtv_bloom_mips_y);
	reset_com_array(g_srv_bloom_mips_y);
	reset_com_array(g_rtv_bloom_mips_x);
	reset_com_array(g_srv_bloom_mips_x);
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

	if (!reshade::get_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality)) {
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	if (!reshade::get_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness)) {
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
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

	// For whatever reason changing Bloom nmips is crashing the game.
	//if (ImGui::SliderInt("Bloom nmips", &g_bloom_nmips, 1, 10)) {
	//	release_com_array(g_rtv_bloom_mips_y);
	//	release_com_array(g_srv_bloom_mips_y);
	//	release_com_array(g_rtv_bloom_mips_x);
	//	release_com_array(g_srv_bloom_mips_x);
	//	g_rtv_bloom_mips_y.resize(g_bloom_nmips);
	//	g_srv_bloom_mips_y.resize(g_bloom_nmips);
	//	g_rtv_bloom_mips_x.resize(g_bloom_nmips);
	//	g_srv_bloom_mips_x.resize(g_bloom_nmips);
	//	g_bloom_sigmas.resize(g_bloom_nmips);
	//}
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
	ImGui::Spacing();

	if (ImGui::SliderFloat("Sharpness", &g_amd_ffx_cas_sharpness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["amd_ffx_cas"_h].reset();
		reshade::set_config_value(nullptr, NAME, "Sharpness", g_amd_ffx_cas_sharpness);
	}
	ImGui::EndDisabled();
	ImGui::Spacing();

	static constexpr std::array gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("GTAO quality", &g_gtao_quality, gtao_quality_items.data(), gtao_quality_items.size())) {
		g_managed_resources.compute_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			if (!reshade::register_addon(hModule)) {
				return FALSE;
			}

			//MessageBoxW(0, L"Debug", L"Attach debugger.", MB_OK);

			init_graphical_upgrade_path();
			read_config();

			// Bloom.
			g_bloom_nmips = 4;
			g_rtv_bloom_mips_y.resize(g_bloom_nmips);
			g_srv_bloom_mips_y.resize(g_bloom_nmips);
			g_rtv_bloom_mips_x.resize(g_bloom_nmips);
			g_srv_bloom_mips_x.resize(g_bloom_nmips);
			g_bloom_sigmas.resize(g_bloom_nmips);
			g_bloom_sigmas[0] = 1.0f; // 1.0 for the first MIP here is fine cause input is prefiltered.
			g_bloom_sigmas[1] = 1.0f;
			g_bloom_sigmas[2] = 1.0f;
			g_bloom_sigmas[3] = 1.0f;

			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::dispatch>(on_dispatch);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::map_buffer_region>(on_map_buffer_region);
			reshade::register_event<reshade::addon_event::unmap_buffer_region>(on_unmap_buffer_region);
			reshade::register_event<reshade::addon_event::create_resource>(on_create_resource);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
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
