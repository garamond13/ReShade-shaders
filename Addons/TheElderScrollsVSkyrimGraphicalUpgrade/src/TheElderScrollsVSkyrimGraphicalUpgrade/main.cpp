#define DEV 0
#define OUTPUT_ASSEMBLY 0
#define SHOW_AO 0
#include "Include/GraphicalUpgrade.h"
#include "Include/GraphicalUpgradeCB.hlsli.h"
#include "DLSS/DLSS.h"

extern "C" __declspec(dllexport) const char* NAME = "TheElderScrollsVSkyrimGraphicalUpgrade";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "v1.0.0";
extern "C" __declspec(dllexport) const char* WEBSITE = "https://github.com/garamond13/ReShade-shaders/tree/main/Addons/TheElderScrollsVSkyrimGraphicalUpgrade";

// Shader hooks.
//

constexpr Shader_hash g_ps_linearize_depth_0xA8887CF9 = { 0xA8887CF9, { 0x73b474e4, 0xe57, 0x4a96, { 0xb4, 0x35, 0xf2, 0xaf, 0x2a, 0x97, 0x87, 0xd6 }}};
constexpr Shader_hash g_ps_ssao_main_0x48823C1C = { 0x48823C1C, { 0x7c8e0be2, 0xd180, 0x4ea6, { 0xa2, 0xfb, 0x70, 0xd6, 0xd3, 0x1b, 0x2a, 0x60 }}};
constexpr Shader_hash g_ps_ssao_denoise_0xEEB0297F = { 0xEEB0297F, { 0x14ed8b34, 0x3fed, 0x42fd, { 0xb1, 0x9c, 0x29, 0x5c, 0x68, 0xfd, 0x19, 0xbd }}};
constexpr Shader_hash g_ps_ssao_denoise_0xFF4B533E = { 0xFF4B533E, { 0x9d6a2397, 0x642d, 0x42ee, { 0xba, 0xc, 0x47, 0x23, 0x5e, 0x6c, 0xe6, 0x1e }}};

constexpr Shader_hash g_ps_downsample_0xFEE901F4 = { 0xFEE901F4, { 0xc330affc, 0xd36b, 0x40db, { 0x80, 0x6f, 0x11, 0xc0, 0xc0, 0xf, 0x89, 0xa4 }}};

constexpr Shader_hash g_ps_bloom_0x1FDE1B31 = { 0x1FDE1B31, { 0x5bfd5e8c, 0xac64, 0x42ed, { 0xa2, 0x1a, 0x98, 0xe6, 0xb1, 0xdb, 0x39, 0xa8 }}};
constexpr Shader_hash g_ps_bloom_0x041258DD = { 0x041258DD, { 0x5e823cce, 0xa679, 0x42aa, { 0xa3, 0x12, 0x48, 0xbe, 0xf3, 0xe7, 0xe4, 0x5f }}};
constexpr Shader_hash g_ps_tonemap_0x936CE1A3 = { 0x936CE1A3, { 0x1d0572d4, 0x7259, 0x4aa1, { 0x9b, 0x8e, 0xf4, 0xd2, 0x39, 0x38, 0xa, 0xbd }}};

constexpr Shader_hash g_ps_taa_0x675543CF = { 0x675543CF, { 0xbcb66fa2, 0xbb03, 0x4c5e, { 0x9e, 0xaf, 0xdf, 0xaa, 0xe0, 0xd0, 0x27, 0xb }}};

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
static void* g_gbuffer_normals;

// GTAO
constexpr size_t GTAO_DEPTH_MIP_LEVELS = 5;
static int g_gtao_quality = 2; // 0 - Low, 1 - Medium, 2 - High, 3 - Very High, 4 - Ultra

// Bloom
static int g_bloom_nmips;
static std::vector<float> g_bloom_sigmas;
static float g_bloom_intensity = 1.0f;

// DLSS
constexpr int g_dlss_flags{
	NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
	NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
};
static NVSDK_NGX_DLSS_Hint_Render_Preset g_dlss_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
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
	hr = ps->GetPrivateData(g_ps_linearize_depth_0xA8887CF9.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_linearize_depth_0xA8887CF9.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["linearize_depth_0xA8887CF9"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["linearize_depth_0xA8887CF9"_h].put(), L"LinearizeDepth_0xA8887CF9_ps.hlsl");
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["linearize_depth_0xA8887CF9"_h].get(), nullptr, 0);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_ssao_main_0x48823C1C.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_main_0x48823C1C.hash) {
		// SRV0 - linearized depth.
		// SRV1 - viewspace normal.
		std::array<ID3D11ShaderResourceView*, 2> srvs;
		ctx->PSGetShaderResources(0, srvs.size(), srvs.data());

		Com_ptr<ID3D11Buffer> cb2;
		ctx->PSGetConstantBuffers(2, 1, cb2.put());

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
		ctx->CSSetShaderResources(0, 1, &srvs[0]);

		ctx->Dispatch((g_swapchain_width + 16 - 1) / 16, (g_swapchain_height + 16 - 1) / 16, 1);

		// Unbind UAVs.
		static constexpr std::array<ID3D11UnorderedAccessView*, GTAO_DEPTH_MIP_LEVELS> uav_nulls_prefilter_depths_pass = {};
		ctx->CSSetUnorderedAccessViews(0, uav_nulls_prefilter_depths_pass.size(), uav_nulls_prefilter_depths_pass.data(), nullptr);

		//

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

		++g_cb_data.gtao_temporal_index;
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
		ctx->CSSetConstantBuffers(2, 1, &cb2);
		ctx->CSSetConstantBuffers(GRAPHICAL_UPGRADE_CB_SLOT, 1, &g_cb);
		ctx->CSSetSamplers(0, 1, &g_managed_resources.samplers["point_clamp"_h]);
		const std::array gtao_main_srvs = { g_managed_resources.shader_resource_views["gtao_prefilter_depths16x16"_h].get(), srvs[1] };
		ctx->CSSetShaderResources(0, gtao_main_srvs.size(), gtao_main_srvs.data());

		ctx->Dispatch((g_swapchain_width + 8 - 1) / 8, (g_swapchain_height + 8 - 1) / 8, 1);

		//

		release_com_array(srvs);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_ssao_denoise_0xEEB0297F.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_denoise_0xEEB0297F.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_ssao_denoise_0xFF4B533E.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_ssao_denoise_0xFF4B533E.hash) {
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

		// Get the original RT and create UAV.
		Com_ptr<ID3D11RenderTargetView> rtv;
		ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
		Com_ptr<ID3D11Resource> resource_rt;
		rtv->GetResource(resource_rt.put());
		Com_ptr<ID3D11UnorderedAccessView> uav;
		ensure(g_device->CreateUnorderedAccessView(resource_rt.get(), nullptr, uav.put()), >= 0);

		#if DEV && SHOW_AO
		g_managed_resources.resources["gtao_denoise_pass2"_h] = resource_rt;
		#endif

		// Bindings.
		ctx->OMSetRenderTargets(0, nullptr, nullptr);
		ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		ctx->CSSetShader(g_managed_resources.compute_shaders["gtao_denoise_pass2"_h].get(), nullptr, 0);
		ctx->CSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["gtao_denoise_pass1"_h]);

		ctx->Dispatch((g_swapchain_width + 8 * 2 - 1) / (8 * 2), (g_swapchain_height + 8 - 1) / 8, 1);

		//

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_downsample_0xFEE901F4.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_downsample_0xFEE901F4.hash) {
		// This should be valid for the bloom.
		ctx->PSGetShaderResources(0, 1, g_managed_resources.shader_resource_views["scene"_h].put());

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["downsample_0xFEE901F4"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["downsample_0xFEE901F4"_h].put(), L"Downsample_0xFEE901F4_ps.hlsl");
		}
	
		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["downsample_0xFEE901F4"_h].get(), nullptr, 0);
	
		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x1FDE1B31.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x1FDE1B31.hash) {
		// Backup Viewports.
		UINT num_viewports;
		ctx->RSGetViewports(&num_viewports, nullptr);
		std::vector<D3D11_VIEWPORT> viewports_original(num_viewports);
		ctx->RSGetViewports(&num_viewports, viewports_original.data());

		// Backup samplers.
		Com_ptr<ID3D11SamplerState> ps_sampler_original;
		ctx->PSGetSamplers(0, 1, ps_sampler_original.put());

		// Backup Blend.
		Com_ptr<ID3D11BlendState> blend_original;
		FLOAT blend_factor_original[4];
		UINT sample_mask_original;
		ctx->OMGetBlendState(blend_original.put(), blend_factor_original, &sample_mask_original);

		// Sanitize scene pass.
		//
		// We linearize scene here as well.
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_sanitize_scene"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_sanitize_scene"_h].put(), L"Bloom_impl.hlsl", "sanitize_scene_ps");
		}

		// Create RT and views.
		[[unlikely]] if (!g_managed_resources.render_target_views["bloom_sanitize_scene"_h]) {
			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = g_swapchain_width;
			tex_desc.Height = g_swapchain_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			Com_ptr<ID3D11Texture2D> tex;
			ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
			ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["bloom_sanitize_scene"_h].put()), >= 0);
			ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["bloom_sanitize_scene"_h].put()), >= 0);
		}

		D3D11_VIEWPORT viewport = {};
		viewport.Width = g_swapchain_width;
		viewport.Height = g_swapchain_height;

		// Bindings
		ctx->OMSetRenderTargets(1, &g_managed_resources.render_target_views["bloom_sanitize_scene"_h], nullptr);
		ctx->RSSetViewports(1, &viewport);
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_sanitize_scene"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["scene"_h]);

		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

		//

		const int bloom_input_width = g_swapchain_width;
		const int bloom_input_height = g_swapchain_height;

		// Create MIPs and views.
		//

		const UINT x_mip0_width = bloom_input_width >> 1;
		const UINT x_mip0_height = bloom_input_height;

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

		const UINT y_mip0_width = bloom_input_width >> 1;
		const UINT y_mip0_height = bloom_input_height >> 1;

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
		
		// Prefilter and downsample pass
		//

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_downsample"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_downsample"_h].put(), L"Bloom_impl.hlsl", "downsample_ps");
		}

		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["bloom_prefilter"_h]) {
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["bloom_prefilter"_h].put(), L"Bloom_impl.hlsl", "prefilter_ps");
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
		g_cb_data.src_size = float2(bloom_input_width, bloom_input_height);
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
		ctx->PSSetShaderResources(0, 1, &g_managed_resources.shader_resource_views["bloom_sanitize_scene"_h]);

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
		ctx->PSSetShader(g_managed_resources.pixel_shaders["bloom_prefilter"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[0]);
		ctx->RSSetViewports(1, &viewports_y[0]);

		// Draw Y pass.
		cmd_list->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);

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
			ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_x[i]);
			ctx->RSSetViewports(1, &viewports_y[i]);

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

		//

		// Restore.
		// May not be necessary, needs testing.
		ctx->OMSetBlendState(blend_original.get(), blend_factor_original, sample_mask_original);
		ctx->RSSetViewports(viewports_original.size(), viewports_original.data());
		ctx->PSSetSamplers(0, 1, &ps_sampler_original);

		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_bloom_0x041258DD.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_bloom_0x041258DD.hash) {
		return true;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_tonemap_0x936CE1A3.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_tonemap_0x936CE1A3.hash) {
		// Create PS.
		[[unlikely]] if (!g_managed_resources.pixel_shaders["tonemap_0x936CE1A3"_h]) {
			const std::string bloom_intensity_str = std::to_string(g_bloom_intensity);
			const D3D_SHADER_MACRO defines[] = {
				{ "BLOOM_INTENSITY", bloom_intensity_str.c_str() },
				{ nullptr, nullptr }
			};
			create_pixel_shader(g_device, g_managed_resources.pixel_shaders["tonemap_0x936CE1A3"_h].put(), L"Tonemap_0x936CE1A3_ps.hlsl", "main", defines);
		}

		// Bindings.
		ctx->PSSetShader(g_managed_resources.pixel_shaders["tonemap_0x936CE1A3"_h].get(), nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &g_srv_bloom_mips_y[0]);

		return false;
	}

	size = sizeof(hash);
	hr = ps->GetPrivateData(g_ps_taa_0x675543CF.guid, &size, &hash);
	if (SUCCEEDED(hr) && hash == g_ps_taa_0x675543CF.hash) {
		if (g_enable_dlss) {
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

			// Get RT.
			Com_ptr<ID3D11RenderTargetView> rtv;
			ctx->OMGetRenderTargets(1, rtv.put(), nullptr);
			Com_ptr<ID3D11Resource> resource_rt;
			rtv->GetResource(resource_rt.put());

			// Create the output resource for DLSS.
			// The original RT has no D3D11_BIND_UNORDERED_ACCESS flag.
			[[unlikely]] if (!g_managed_resources.textures_2d["dlss_output"_h]) {
				// Get the original RT texture description.
				ensure(resource_rt->QueryInterface(g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
				D3D11_TEXTURE2D_DESC tex_desc;
				g_managed_resources.textures_2d["dlss_output"_h]->GetDesc(&tex_desc);

				// Create DLSS output.
				tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
				ensure(g_device->CreateTexture2D(&tex_desc, nullptr, g_managed_resources.textures_2d["dlss_output"_h].put()), >= 0);
			}

			#if DEV && SHOW_AO
			resource_scene = g_managed_resources.resources["gtao_denoise_pass2"_h];
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

			ctx->CopyResource(resource_rt.get(), g_managed_resources.textures_2d["dlss_output"_h].get());

			release_com_array(srvs);

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
				case g_ps_linearize_depth_0xA8887CF9.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_linearize_depth_0xA8887CF9.guid, sizeof(g_ps_linearize_depth_0xA8887CF9.hash), &g_ps_linearize_depth_0xA8887CF9.hash), >= 0);
					return;
				case g_ps_ssao_main_0x48823C1C.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_main_0x48823C1C.guid, sizeof(g_ps_ssao_main_0x48823C1C.hash), &g_ps_ssao_main_0x48823C1C.hash), >= 0);
					return;
				case g_ps_ssao_denoise_0xEEB0297F.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_denoise_0xEEB0297F.guid, sizeof(g_ps_ssao_denoise_0xEEB0297F.hash), &g_ps_ssao_denoise_0xEEB0297F.hash), >= 0);
					return;
				case g_ps_ssao_denoise_0xFF4B533E.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_ssao_denoise_0xFF4B533E.guid, sizeof(g_ps_ssao_denoise_0xFF4B533E.hash), &g_ps_ssao_denoise_0xFF4B533E.hash), >= 0);
					return;
				case g_ps_downsample_0xFEE901F4.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_downsample_0xFEE901F4.guid, sizeof(g_ps_downsample_0xFEE901F4.hash), &g_ps_downsample_0xFEE901F4.hash), >= 0);
					return;
				case g_ps_bloom_0x1FDE1B31.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x1FDE1B31.guid, sizeof(g_ps_bloom_0x1FDE1B31.hash), &g_ps_bloom_0x1FDE1B31.hash), >= 0);
					return;
				case g_ps_bloom_0x041258DD.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_bloom_0x041258DD.guid, sizeof(g_ps_bloom_0x041258DD.hash), &g_ps_bloom_0x041258DD.hash), >= 0);
					return;
				case g_ps_tonemap_0x936CE1A3.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_tonemap_0x936CE1A3.guid, sizeof(g_ps_tonemap_0x936CE1A3.hash), &g_ps_tonemap_0x936CE1A3.hash), >= 0);
					return;
				case g_ps_taa_0x675543CF.hash:
					ensure(((ID3D11PixelShader*)pipeline.handle)->SetPrivateData(g_ps_taa_0x675543CF.guid, sizeof(g_ps_taa_0x675543CF.hash), &g_ps_taa_0x675543CF.hash), >= 0);
					return;
			}
		}
	}
}

// Fix/improve g-buffer normals. Upgrading RTs in this game is very much impossible, so we have to do it this way.
// The game packs g-buffer normals in RG channels, but it's using DXGI_FORMAT_R8G8B8A8_UNORM instead of DXGI_FORMAT_R16G16_UNORM as in Fallout 4.
// DXGI_FORMAT_R16G16_UNORM breaks water (other channels used at some point?), so we use DXGI_FORMAT_R16G16B16A16_UNORM instead.
//

static bool on_clear_render_target_view(reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, const float color[4], uint32_t rect_count, const reshade::api::rect* rects)
{
	// Check color first, optimization.
	if (color[0] == 0.5f && color[1] == 0.5f && !color[2] && !color[3]) {
		auto native_rtv = (ID3D11RenderTargetView*)rtv.handle;
		Com_ptr<ID3D11Resource> resource;
		native_rtv->GetResource(resource.put());
		Com_ptr<ID3D11Texture2D> tex;
		auto hr = resource->QueryInterface(tex.put());
		if (SUCCEEDED(hr)) {
			D3D11_TEXTURE2D_DESC tex_desc;
			tex->GetDesc(&tex_desc);
			if (tex_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
				// Create new g-buffer normals views.
				[[unlikely]] if (!g_managed_resources.render_target_views["new_gbuffer_normals"_h]) {
					tex_desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
					ensure(g_device->CreateTexture2D(&tex_desc, nullptr, tex.put()), >= 0);
					ensure(g_device->CreateRenderTargetView(tex.get(), nullptr, g_managed_resources.render_target_views["new_gbuffer_normals"_h].put()), >= 0);
					ensure(g_device->CreateShaderResourceView(tex.get(), nullptr, g_managed_resources.shader_resource_views["new_gbuffer_normals"_h].put()), >= 0);
				}

				// Clear new g-buffer normals to the same color as the original would be.
				auto ctx = (ID3D11DeviceContext*)cmd_list->get_native();
				ctx->ClearRenderTargetView(g_managed_resources.render_target_views["new_gbuffer_normals"_h].get(), color);

				// We need to track original gbuffer normals later.
				((ID3D11RenderTargetView*)rtv.handle)->GetResource(resource.put());
				g_gbuffer_normals = resource.get();

				return true;
			}
		}
	}
	return false;
}

static void on_push_descriptors(reshade::api::command_list* cmd_list, reshade::api::shader_stage stages, reshade::api::pipeline_layout layout, uint32_t layout_param, const reshade::api::descriptor_table_update& update)
{
	if (update.type == reshade::api::descriptor_type::shader_resource_view) {
		auto srvs = (const reshade::api::resource_view*)update.descriptors;
		std::vector<reshade::api::resource_view> new_srvs(srvs, srvs + update.count);
		auto new_update = update;
		new_update.descriptors = new_srvs.data();
		Com_ptr<ID3D11Resource> resource;
		for (uint32_t i = 0; i < new_update.count; ++i) {
			if (new_srvs[i].handle) {
				((ID3D11ShaderResourceView*)new_srvs[i].handle)->GetResource(resource.put());
				if (resource == g_gbuffer_normals) {
					new_srvs[i].handle = (uintptr_t)g_managed_resources.shader_resource_views["new_gbuffer_normals"_h].get();
					cmd_list->push_descriptors(stages, layout, layout_param, new_update);
					break;
				}
			}
		}
	}
}

static void on_bind_render_targets_and_depth_stencil(reshade::api::command_list* cmd_list, uint32_t count, const reshade::api::resource_view* rtvs, reshade::api::resource_view dsv)
{
	std::vector<reshade::api::resource_view> new_rtvs(rtvs, rtvs + count);
	Com_ptr<ID3D11Resource> resource;
	for (uint32_t i = 0; i < count; ++i) {
		if (new_rtvs[i].handle) {
			((ID3D11RenderTargetView*)new_rtvs[i].handle)->GetResource(resource.put());
			if (resource == g_gbuffer_normals) {
				new_rtvs[i].handle = (uintptr_t)g_managed_resources.render_target_views["new_gbuffer_normals"_h].get();
				cmd_list->bind_render_targets_and_depth_stencil(count, new_rtvs.data(), dsv);
				break;
			}
		}
	}
}

//

static void on_map_buffer_region(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data)
{
	auto buffer = (ID3D11Buffer*)resource.handle;
	D3D11_BUFFER_DESC desc;
	buffer->GetDesc(&desc);

	// This should be reliable, we want TAA CB2.
	if (desc.BindFlags == D3D11_BIND_CONSTANT_BUFFER && desc.ByteWidth == 3840) {
		g_mapped_cb_handle = resource.handle;
		g_mapped_cb_data = *data;
	}
}

static void on_unmap_buffer_region(reshade::api::device* device, reshade::api::resource resource)
{
	if (g_mapped_cb_handle == resource.handle) {
		auto data = (float4*)g_mapped_cb_data;

		// Should be 8 long Halton(2,3) sequence in UV offsets.
		g_jitter_x = data[1].x;
		g_jitter_y = data[1].y;

		g_mapped_cb_handle = 0;
	}
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

	g_managed_resources.render_target_views["new_gbuffer_normals"_h].reset();
	g_managed_resources.textures_2d["dlss_output"_h].reset();

	// GTAO.
	g_managed_resources.compute_shaders["gtao_prefilter_depths16x16"_h].reset();
	reset_com_array(g_uav_gtao_prefilter_depths16x16);
	g_managed_resources.compute_shaders["gtao_main_pass"_h].reset();
	g_managed_resources.unordered_access_views["gtao_main_pass"_h].reset();
	g_managed_resources.compute_shaders["gtao_denoise_pass1"_h].reset();
	g_managed_resources.unordered_access_views["gtao_denoise_pass1"_h].reset();
	g_managed_resources.compute_shaders["gtao_denoise_pass2"_h].reset();

	// Bloom.
	g_managed_resources.render_target_views["bloom_sanitize_scene"_h].reset();
	reset_com_array(g_rtv_bloom_mips_y);
	reset_com_array(g_srv_bloom_mips_y);
	reset_com_array(g_rtv_bloom_mips_x);
	reset_com_array(g_srv_bloom_mips_x);

	//
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
	if (!reshade::get_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity)) {
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
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
		g_managed_resources.pixel_shaders["bloom_sanitize_scene"_h].reset();
		g_managed_resources.pixel_shaders["bloom_prefilter"_h].reset();
		g_managed_resources.pixel_shaders["tonemap_0x936CE1A3"_h].reset();
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

	static constexpr std::array gtao_quality_items = { "Low", "Medium", "High", "Very High", "Ultra" };
	if (ImGui::Combo("GTAO quality", &g_gtao_quality, gtao_quality_items.data(), gtao_quality_items.size())) {
		g_managed_resources.compute_shaders["gtao_main_pass"_h].reset();
		reshade::set_config_value(nullptr, NAME, "GTAOQuality", g_gtao_quality);
	}
	ImGui::Spacing();

	if (ImGui::SliderFloat("Bloom intensity", &g_bloom_intensity, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
		g_managed_resources.pixel_shaders["tonemap_0x936CE1A3"_h].reset();
		reshade::set_config_value(nullptr, NAME, "BloomIntensity", g_bloom_intensity);
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

			reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
			reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
			reshade::register_event<reshade::addon_event::clear_render_target_view>(on_clear_render_target_view);
			reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
			reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
			reshade::register_event<reshade::addon_event::map_buffer_region>(on_map_buffer_region);
			reshade::register_event<reshade::addon_event::unmap_buffer_region>(on_unmap_buffer_region);
			reshade::register_event<reshade::addon_event::create_sampler>(on_create_sampler);
			reshade::register_event<reshade::addon_event::set_fullscreen_state>(on_set_fullscreen_state);
			reshade::register_event<reshade::addon_event::create_swapchain>(on_create_swapchain);
			reshade::register_event<reshade::addon_event::init_swapchain>(on_init_swapchain);
			reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
			reshade::register_overlay(nullptr, draw_settings_overlay);
			break;
		case DLL_PROCESS_DETACH:
			reshade::unregister_addon(hModule);
			break;
	}
	return TRUE;
}
