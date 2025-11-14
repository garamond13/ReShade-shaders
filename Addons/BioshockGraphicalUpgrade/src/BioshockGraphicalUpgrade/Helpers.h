#pragma once

#include "Common.h"

class RT_views
{
public:

	RT_views(const RT_views&) = delete;
	RT_views& operator=(const RT_views&) = delete;

public:

	RT_views(RT_views&& other) noexcept
		: rtv(other.rtv),
		srv(other.srv)
	{
		other.rtv = nullptr;
		other.srv = nullptr;
	}

	RT_views(ID3D10Device* device, const D3D10_TEXTURE2D_DESC& tex_desc)
	{
		ID3D10Texture2D* tex;
		ensure(device->CreateTexture2D(&tex_desc, nullptr, &tex), >= 0);
		ensure(device->CreateShaderResourceView(tex, nullptr, &srv), >= 0);
		ensure(device->CreateRenderTargetView(tex, nullptr, &rtv), >= 0);
		tex->Release();
	}

	~RT_views() noexcept
	{
		release();
	}

public:

	RT_views& operator=(RT_views&& other) noexcept
	{
		if (this != &other) {
			release();
			rtv = other.rtv;
			srv = other.srv;
			other.rtv = nullptr;
			other.srv = nullptr;
		}
		return *this;
	}

public:

	ID3D10RenderTargetView* get_rtv() const noexcept
	{
		return rtv;
	}

	ID3D10ShaderResourceView* get_srv() const noexcept
	{
		return srv;
	}

	ID3D10RenderTargetView* const* get_rtv_address() const noexcept
	{
		return &rtv;
	}

	ID3D10ShaderResourceView* const* get_srv_address() const noexcept{
		return &srv;
	}

private:

	void release() noexcept
	{
		if (rtv) {
			rtv->Release();
		}
		if (srv) {
			srv->Release();
		}
	}

	ID3D10RenderTargetView* rtv = nullptr;
	ID3D10ShaderResourceView* srv = nullptr;
};

template<typename... Args>
void log_info(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::info, msg.c_str());
}

template<typename... Args>
void log_error(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::error, msg.c_str());
}

template<typename... Args>
void log_debug(std::string_view fmt, Args&&... args)
{
	const std::string msg = std::vformat(fmt, std::make_format_args(args...));
	reshade::log::message(reshade::log::level::debug, msg.c_str());
}

constexpr D3D10_DEPTH_STENCIL_DESC default_D3D10_DEPTH_STENCIL_DESC() noexcept
{
	D3D10_DEPTH_STENCIL_DESC desc = {
		.DepthEnable = TRUE,
		.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL,
		.DepthFunc = D3D10_COMPARISON_LESS,
		.StencilEnable = FALSE,
		.StencilReadMask = D3D10_DEFAULT_STENCIL_READ_MASK,
		.StencilWriteMask = D3D10_DEFAULT_STENCIL_WRITE_MASK,
		.FrontFace = {
			.StencilFailOp = D3D10_STENCIL_OP_KEEP,
			.StencilDepthFailOp = D3D10_STENCIL_OP_KEEP,
			.StencilPassOp = D3D10_STENCIL_OP_KEEP,
			.StencilFunc = D3D10_COMPARISON_ALWAYS
		},
		.BackFace = {
			.StencilFailOp = D3D10_STENCIL_OP_KEEP,
			.StencilDepthFailOp = D3D10_STENCIL_OP_KEEP,
			.StencilPassOp = D3D10_STENCIL_OP_KEEP,
			.StencilFunc = D3D10_COMPARISON_ALWAYS
		}
	};
	return desc;
}

constexpr D3D10_BLEND_DESC default_D3D10_BLEND_DESC() noexcept
{
	D3D10_BLEND_DESC desc = {
		.AlphaToCoverageEnable = FALSE,
		.BlendEnable = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
		.SrcBlend = D3D10_BLEND_ONE,
		.DestBlend = D3D10_BLEND_ZERO,
		.BlendOp = D3D10_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D10_BLEND_ONE,
		.DestBlendAlpha = D3D10_BLEND_ZERO,
		.BlendOpAlpha = D3D10_BLEND_OP_ADD,
		.RenderTargetWriteMask = { D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL, D3D10_COLOR_WRITE_ENABLE_ALL }
	};
	return desc;
}

inline auto to_string(reshade::api::format format)
{
	switch(format) {
		case reshade::api::format::unknown:
			return "unknown";
		case reshade::api::format::r1_unorm:
			return "r1_unorm";
		case reshade::api::format::r8_typeless:
			return "r8_typeless";
		case reshade::api::format::r8_unorm:
			return "r8_unorm";
		case reshade::api::format::r8_uint:
			return "r8_uint";
		case reshade::api::format::r8_snorm:
			return "r8_snorm";
		case reshade::api::format::r8_sint:
			return "r8_sint";
		case reshade::api::format::l8_unorm:
			return "l8_unorm";
		case reshade::api::format::a8_unorm:
			return "a8_unorm";
		case reshade::api::format::r8g8_typeless:
			return "r8g8_typeless";
		case reshade::api::format::r8g8_unorm:
			return "r8g8_unorm";
		case reshade::api::format::r8g8_uint:
			return "r8g8_uint";
		case reshade::api::format::r8g8_snorm:
			return "r8g8_snorm";
		case reshade::api::format::r8g8_sint:
			return "r8g8_sint";
		case reshade::api::format::l8a8_unorm:
			return "l8a8_unorm";
		case reshade::api::format::r8g8b8_typeless:
			return "r8g8b8_typeless";
		case reshade::api::format::r8g8b8_unorm:
			return "r8g8b8_unorm";
		case reshade::api::format::r8g8b8_unorm_srgb:
			return "r8g8b8_unorm_srgb";
		case reshade::api::format::r8g8b8_uint:
			return "r8g8b8_uint";
		case reshade::api::format::r8g8b8_snorm:
			return "r8g8b8_snorm";
		case reshade::api::format::r8g8b8_sint:
			return "r8g8b8_sint";
		case reshade::api::format::b8g8r8_typeless:
			return "b8g8r8_typeless";
		case reshade::api::format::b8g8r8_unorm:
			return "b8g8r8_unorm";
		case reshade::api::format::b8g8r8_unorm_srgb:
			return "b8g8r8_unorm_srgb";
		case reshade::api::format::r8g8b8a8_typeless:
			return "r8g8b8a8_typeless";
		case reshade::api::format::r8g8b8a8_unorm:
			return "r8g8b8a8_unorm";
		case reshade::api::format::r8g8b8a8_unorm_srgb:
			return "r8g8b8a8_unorm_srgb";
		case reshade::api::format::r8g8b8a8_uint:
			return "r8g8b8a8_uint";
		case reshade::api::format::r8g8b8a8_snorm:
			return "r8g8b8a8_snorm";
		case reshade::api::format::r8g8b8a8_sint:
			return "r8g8b8a8_sint";
		case reshade::api::format::r8g8b8x8_unorm:
			return "r8g8b8x8_unorm";
		case reshade::api::format::r8g8b8x8_unorm_srgb:
			return "r8g8b8x8_unorm_srgb";
		case reshade::api::format::b8g8r8a8_typeless:
			return "b8g8r8a8_typeless";
		case reshade::api::format::b8g8r8a8_unorm:
			return "b8g8r8a8_unorm";
		case reshade::api::format::b8g8r8a8_unorm_srgb:
			return "b8g8r8a8_unorm_srgb";
		case reshade::api::format::b8g8r8x8_typeless:
			return "b8g8r8x8_typeless";
		case reshade::api::format::b8g8r8x8_unorm:
			return "b8g8r8x8_unorm";
		case reshade::api::format::b8g8r8x8_unorm_srgb:
			return "b8g8r8x8_unorm_srgb";
		case reshade::api::format::r10g10b10a2_typeless:
			return "r10g10b10a2_typeless";
		case reshade::api::format::r10g10b10a2_unorm:
			return "r10g10b10a2_unorm";
		case reshade::api::format::r10g10b10a2_uint:
			return "r10g10b10a2_uint";
		case reshade::api::format::r10g10b10a2_xr_bias:
			return "r10g10b10a2_xr_bias";
		case reshade::api::format::b10g10r10a2_typeless:
			return "b10g10r10a2_typeless";
		case reshade::api::format::b10g10r10a2_unorm:
			return "b10g10r10a2_unorm";
		case reshade::api::format::b10g10r10a2_uint:
			return "b10g10r10a2_uint";
		case reshade::api::format::r16_typeless:
			return "r16_typeless";
		case reshade::api::format::r16_float:
			return "r16_float";
		case reshade::api::format::r16_unorm:
			return "r16_unorm";
		case reshade::api::format::r16_uint:
			return "r16_uint";
		case reshade::api::format::r16_snorm:
			return "r16_snorm";
		case reshade::api::format::r16_sint:
			return "r16_sint";
		case reshade::api::format::l16_unorm:
			return "l16_unorm";
		case reshade::api::format::l16a16_unorm:
			return "l16a16_unorm";
		case reshade::api::format::r16g16_typeless:
			return "r16g16_typeless";
		case reshade::api::format::r16g16_float:
			return "r16g16_float";
		case reshade::api::format::r16g16_unorm:
			return "r16g16_unorm";
		case reshade::api::format::r16g16_uint:
			return "r16g16_uint";
		case reshade::api::format::r16g16_snorm:
			return "r16g16_snorm";
		case reshade::api::format::r16g16_sint:
			return "r16g16_sint";
		case reshade::api::format::r16g16b16_typeless:
			return "r16g16b16_typeless";
		case reshade::api::format::r16g16b16_float:
			return "r16g16b16_float";
		case reshade::api::format::r16g16b16_unorm:
			return "r16g16b16_unorm";
		case reshade::api::format::r16g16b16_uint:
			return "r16g16b16_uint";
		case reshade::api::format::r16g16b16_snorm:
			return "r16g16b16_snorm";
		case reshade::api::format::r16g16b16_sint:
			return "r16g16b16_sint";
		case reshade::api::format::r16g16b16a16_typeless:
			return "r16g16b16a16_typeless";
		case reshade::api::format::r16g16b16a16_float:
			return "r16g16b16a16_float";
		case reshade::api::format::r16g16b16a16_unorm:
			return "r16g16b16a16_unorm";
		case reshade::api::format::r16g16b16a16_uint:
			return "r16g16b16a16_uint";
		case reshade::api::format::r16g16b16a16_snorm:
			return "r16g16b16a16_snorm";
		case reshade::api::format::r16g16b16a16_sint:
			return "r16g16b16a16_sint";
		case reshade::api::format::r32_typeless:
			return "r32_typeless";
		case reshade::api::format::r32_float:
			return "r32_float";
		case reshade::api::format::r32_uint:
			return "r32_uint";
		case reshade::api::format::r32_sint:
			return "r32_sint";
		case reshade::api::format::r32g32_typeless:
			return "r32g32_typeless";
		case reshade::api::format::r32g32_float:
			return "r32g32_float";
		case reshade::api::format::r32g32_uint:
			return "r32g32_uint";
		case reshade::api::format::r32g32_sint:
			return "r32g32_sint";
		case reshade::api::format::r32g32b32_typeless:
			return "r32g32b32_typeless";
		case reshade::api::format::r32g32b32_float:
			return "r32g32b32_float";
		case reshade::api::format::r32g32b32_uint:
			return "r32g32b32_uint";
		case reshade::api::format::r32g32b32_sint:
			return "r32g32b32_sint";
		case reshade::api::format::r32g32b32a32_typeless:
			return "r32g32b32a32_typeless";
		case reshade::api::format::r32g32b32a32_float:
			return "r32g32b32a32_float";
		case reshade::api::format::r32g32b32a32_uint:
			return "r32g32b32a32_uint";
		case reshade::api::format::r32g32b32a32_sint:
			return "r32g32b32a32_sint";
		case reshade::api::format::r9g9b9e5:
			return "r9g9b9e5";
		case reshade::api::format::r11g11b10_float:
			return "r11g11b10_float";
		case reshade::api::format::b5g6r5_unorm:
			return "b5g6r5_unorm";
		case reshade::api::format::b5g5r5a1_unorm:
			return "b5g5r5a1_unorm";
		case reshade::api::format::b5g5r5x1_unorm:
			return "b5g5r5x1_unorm";
		case reshade::api::format::b4g4r4a4_unorm:
			return "b4g4r4a4_unorm";
		case reshade::api::format::a4b4g4r4_unorm:
			return "a4b4g4r4_unorm";
		case reshade::api::format::s8_uint:
			return "s8_uint";
		case reshade::api::format::d16_unorm:
			return "d16_unorm";
		case reshade::api::format::d16_unorm_s8_uint:
			return "d16_unorm_s8_uint";
		case reshade::api::format::d24_unorm_x8_uint:
			return "d24_unorm_x8_uint";
		case reshade::api::format::d24_unorm_s8_uint:
			return "d24_unorm_s8_uint";
		case reshade::api::format::d32_float:
			return "d32_float";
		case reshade::api::format::d32_float_s8_uint:
			return "d32_float_s8_uint";
		case reshade::api::format::r24_g8_typeless:
			return "r24_g8_typeless";
		case reshade::api::format::r24_unorm_x8_uint:
			return "r24_unorm_x8_uint";
		case reshade::api::format::x24_unorm_g8_uint:
			return "x24_unorm_g8_uint";
		case reshade::api::format::r32_g8_typeless:
			return "r32_g8_typeless";
		case reshade::api::format::r32_float_x8_uint:
			return "r32_float_x8_uint";
		case reshade::api::format::x32_float_g8_uint:
			return "x32_float_g8_uint";
		case reshade::api::format::bc1_typeless:
			return "bc1_typeless";
		case reshade::api::format::bc1_unorm:
			return "bc1_unorm";
		case reshade::api::format::bc1_unorm_srgb:
			return "bc1_unorm_srgb";
		case reshade::api::format::bc2_typeless:
			return "bc2_typeless";
		case reshade::api::format::bc2_unorm:
			return "bc2_unorm";
		case reshade::api::format::bc2_unorm_srgb:
			return "bc2_unorm_srgb";
		case reshade::api::format::bc3_typeless:
			return "bc3_typeless";
		case reshade::api::format::bc3_unorm:
			return "bc3_unorm";
		case reshade::api::format::bc3_unorm_srgb:
			return "bc3_unorm_srgb";
		case reshade::api::format::bc4_typeless:
			return "bc4_typeless";
		case reshade::api::format::bc4_unorm:
			return "bc4_unorm";
		case reshade::api::format::bc4_snorm:
			return "bc4_snorm";
		case reshade::api::format::bc5_typeless:
			return "bc5_typeless";
		case reshade::api::format::bc5_unorm:
			return "bc5_unorm";
		case reshade::api::format::bc5_snorm:
			return "bc5_snorm";
		case reshade::api::format::bc6h_typeless:
			return "bc6h_typeless";
		case reshade::api::format::bc6h_ufloat:
			return "bc6h_ufloat";
		case reshade::api::format::bc6h_sfloat:
			return "bc6h_sfloat";
		case reshade::api::format::bc7_typeless:
			return "bc7_typeless";
		case reshade::api::format::bc7_unorm:
			return "bc7_unorm";
		case reshade::api::format::bc7_unorm_srgb:
			return "bc7_unorm_srgb";
		case reshade::api::format::r8g8_b8g8_unorm:
			return "r8g8_b8g8_unorm";
		case reshade::api::format::g8r8_g8b8_unorm:
			return "g8r8_g8b8_unorm";
		case reshade::api::format::intz:
			return "intz";
		default:
			return "";
		}
}

inline auto to_string(DXGI_FORMAT format)
{
	switch (format) {
		case DXGI_FORMAT_UNKNOWN:
			return "DXGI_FORMAT_UNKNOWN";
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			return "DXGI_FORMAT_R32G32B32A32_TYPELESS";
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return "DXGI_FORMAT_R32G32B32A32_FLOAT";
		case DXGI_FORMAT_R32G32B32A32_UINT:
			return "DXGI_FORMAT_R32G32B32A32_UINT";
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return "DXGI_FORMAT_R32G32B32A32_SINT";
		case DXGI_FORMAT_R32G32B32_TYPELESS:
			return "DXGI_FORMAT_R32G32B32_TYPELESS";
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return "DXGI_FORMAT_R32G32B32_FLOAT";
		case DXGI_FORMAT_R32G32B32_UINT:
			return "DXGI_FORMAT_R32G32B32_UINT";
		case DXGI_FORMAT_R32G32B32_SINT:
			return "DXGI_FORMAT_R32G32B32_SINT";
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			return "DXGI_FORMAT_R16G16B16A16_TYPELESS";
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return "DXGI_FORMAT_R16G16B16A16_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return "DXGI_FORMAT_R16G16B16A16_UNORM";
		case DXGI_FORMAT_R16G16B16A16_UINT:
			return "DXGI_FORMAT_R16G16B16A16_UINT";
		case DXGI_FORMAT_R16G16B16A16_SNORM:
			return "DXGI_FORMAT_R16G16B16A16_SNORM";
		case DXGI_FORMAT_R16G16B16A16_SINT:
			return "DXGI_FORMAT_R16G16B16A16_SINT";
		case DXGI_FORMAT_R32G32_TYPELESS:
			return "DXGI_FORMAT_R32G32_TYPELESS";
		case DXGI_FORMAT_R32G32_FLOAT:
			return "DXGI_FORMAT_R32G32_FLOAT";
		case DXGI_FORMAT_R32G32_UINT:
			return "DXGI_FORMAT_R32G32_UINT";
		case DXGI_FORMAT_R32G32_SINT:
			return "DXGI_FORMAT_R32G32_SINT";
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return "DXGI_FORMAT_R32G8X24_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return "DXGI_FORMAT_D32_FLOAT_S8X24_UINT";
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			return "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS";
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT";
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			return "DXGI_FORMAT_R10G10B10A2_TYPELESS";
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			return "DXGI_FORMAT_R10G10B10A2_UNORM";
		case DXGI_FORMAT_R10G10B10A2_UINT:
			return "DXGI_FORMAT_R10G10B10A2_UINT";
		case DXGI_FORMAT_R11G11B10_FLOAT:
			return "DXGI_FORMAT_R11G11B10_FLOAT";
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			return "DXGI_FORMAT_R8G8B8A8_TYPELESS";
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return "DXGI_FORMAT_R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R8G8B8A8_UINT:
			return "DXGI_FORMAT_R8G8B8A8_UINT";
		case DXGI_FORMAT_R8G8B8A8_SNORM:
			return "DXGI_FORMAT_R8G8B8A8_SNORM";
		case DXGI_FORMAT_R8G8B8A8_SINT:
			return "DXGI_FORMAT_R8G8B8A8_SINT";
		case DXGI_FORMAT_R16G16_TYPELESS:
			return "DXGI_FORMAT_R16G16_TYPELESS";
		case DXGI_FORMAT_R16G16_FLOAT:
			return "DXGI_FORMAT_R16G16_FLOAT";
		case DXGI_FORMAT_R16G16_UNORM:
			return "DXGI_FORMAT_R16G16_UNORM";
		case DXGI_FORMAT_R16G16_UINT:
			return "DXGI_FORMAT_R16G16_UINT";
		case DXGI_FORMAT_R16G16_SNORM:
			return "DXGI_FORMAT_R16G16_SNORM";
		case DXGI_FORMAT_R16G16_SINT:
			return "DXGI_FORMAT_R16G16_SINT";
		case DXGI_FORMAT_R32_TYPELESS:
			return "DXGI_FORMAT_R32_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT:
			return "DXGI_FORMAT_D32_FLOAT";
		case DXGI_FORMAT_R32_FLOAT:
			return "DXGI_FORMAT_R32_FLOAT";
		case DXGI_FORMAT_R32_UINT:
			return "DXGI_FORMAT_R32_UINT";
		case DXGI_FORMAT_R32_SINT:
			return "DXGI_FORMAT_R32_SINT";
		case DXGI_FORMAT_R24G8_TYPELESS:
			return "DXGI_FORMAT_R24G8_TYPELESS";
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return "DXGI_FORMAT_D24_UNORM_S8_UINT";
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			return "DXGI_FORMAT_R24_UNORM_X8_TYPELESS";
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return "DXGI_FORMAT_X24_TYPELESS_G8_UINT";
		case DXGI_FORMAT_R8G8_TYPELESS:
			return "DXGI_FORMAT_R8G8_TYPELESS";
		case DXGI_FORMAT_R8G8_UNORM:
			return "DXGI_FORMAT_R8G8_UNORM";
		case DXGI_FORMAT_R8G8_UINT:
			return "DXGI_FORMAT_R8G8_UINT";
		case DXGI_FORMAT_R8G8_SNORM:
			return "DXGI_FORMAT_R8G8_SNORM";
		case DXGI_FORMAT_R8G8_SINT:
			return "DXGI_FORMAT_R8G8_SINT";
		case DXGI_FORMAT_R16_TYPELESS:
			return "DXGI_FORMAT_R16_TYPELESS";
		case DXGI_FORMAT_R16_FLOAT:
			return "DXGI_FORMAT_R16_FLOAT";
		case DXGI_FORMAT_D16_UNORM:
			return "DXGI_FORMAT_D16_UNORM";
		case DXGI_FORMAT_R16_UNORM:
			return "DXGI_FORMAT_R16_UNORM";
		case DXGI_FORMAT_R16_UINT:
			return "DXGI_FORMAT_R16_UINT";
		case DXGI_FORMAT_R16_SNORM:
			return "DXGI_FORMAT_R16_SNORM";
		case DXGI_FORMAT_R16_SINT:
			return "DXGI_FORMAT_R16_SINT";
		case DXGI_FORMAT_R8_TYPELESS:
			return "DXGI_FORMAT_R8_TYPELESS";
		case DXGI_FORMAT_R8_UNORM:
			return "DXGI_FORMAT_R8_UNORM";
		case DXGI_FORMAT_R8_UINT:
			return "DXGI_FORMAT_R8_UINT";
		case DXGI_FORMAT_R8_SNORM:
			return "DXGI_FORMAT_R8_SNORM";
		case DXGI_FORMAT_R8_SINT:
			return "DXGI_FORMAT_R8_SINT";
		case DXGI_FORMAT_A8_UNORM:
			return "DXGI_FORMAT_A8_UNORM";
		case DXGI_FORMAT_R1_UNORM:
			return "DXGI_FORMAT_R1_UNORM";
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			return "DXGI_FORMAT_R9G9B9E5_SHAREDEXP";
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
			return "DXGI_FORMAT_R8G8_B8G8_UNORM";
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
			return "DXGI_FORMAT_G8R8_G8B8_UNORM";
		case DXGI_FORMAT_BC1_TYPELESS:
			return "DXGI_FORMAT_BC1_TYPELESS";
		case DXGI_FORMAT_BC1_UNORM:
			return "DXGI_FORMAT_BC1_UNORM";
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return "DXGI_FORMAT_BC1_UNORM_SRGB";
		case DXGI_FORMAT_BC2_TYPELESS:
			return "DXGI_FORMAT_BC2_TYPELESS";
		case DXGI_FORMAT_BC2_UNORM:
			return "DXGI_FORMAT_BC2_UNORM";
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return "DXGI_FORMAT_BC2_UNORM_SRGB";
		case DXGI_FORMAT_BC3_TYPELESS:
			return "DXGI_FORMAT_BC3_TYPELESS";
		case DXGI_FORMAT_BC3_UNORM:
			return "DXGI_FORMAT_BC3_UNORM";
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return "DXGI_FORMAT_BC3_UNORM_SRGB";
		case DXGI_FORMAT_BC4_TYPELESS:
			return "DXGI_FORMAT_BC4_TYPELESS";
		case DXGI_FORMAT_BC4_UNORM:
			return "DXGI_FORMAT_BC4_UNORM";
		case DXGI_FORMAT_BC4_SNORM:
			return "DXGI_FORMAT_BC4_SNORM";
		case DXGI_FORMAT_BC5_TYPELESS:
			return "DXGI_FORMAT_BC5_TYPELESS";
		case DXGI_FORMAT_BC5_UNORM:
			return "DXGI_FORMAT_BC5_UNORM";
		case DXGI_FORMAT_BC5_SNORM:
			return "DXGI_FORMAT_BC5_SNORM";
		case DXGI_FORMAT_B5G6R5_UNORM:
			return "DXGI_FORMAT_B5G6R5_UNORM";
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return "DXGI_FORMAT_B5G5R5A1_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return "DXGI_FORMAT_B8G8R8A8_UNORM";
		case DXGI_FORMAT_B8G8R8X8_UNORM:
			return "DXGI_FORMAT_B8G8R8X8_UNORM";
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			return "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM";
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			return "DXGI_FORMAT_B8G8R8A8_TYPELESS";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			return "DXGI_FORMAT_B8G8R8X8_TYPELESS";
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB";
		case DXGI_FORMAT_BC6H_TYPELESS:
			return "DXGI_FORMAT_BC6H_TYPELESS";
		case DXGI_FORMAT_BC6H_UF16:
			return "DXGI_FORMAT_BC6H_UF16";
		case DXGI_FORMAT_BC6H_SF16:
			return "DXGI_FORMAT_BC6H_SF16";
		case DXGI_FORMAT_BC7_TYPELESS:
			return "DXGI_FORMAT_BC7_TYPELESS";
		case DXGI_FORMAT_BC7_UNORM:
			return "DXGI_FORMAT_BC7_UNORM";
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return "DXGI_FORMAT_BC7_UNORM_SRGB";
		case DXGI_FORMAT_AYUV:
			return "DXGI_FORMAT_AYUV";
		case DXGI_FORMAT_Y410:
			return "DXGI_FORMAT_Y410";
		case DXGI_FORMAT_Y416:
			return "DXGI_FORMAT_Y416";
		case DXGI_FORMAT_NV12:
			return "DXGI_FORMAT_NV12";
		case DXGI_FORMAT_P010:
			return "DXGI_FORMAT_P010";
		case DXGI_FORMAT_P016:
			return "DXGI_FORMAT_P016";
		case DXGI_FORMAT_420_OPAQUE:
			return "DXGI_FORMAT_420_OPAQUE";
		case DXGI_FORMAT_YUY2:
			return "DXGI_FORMAT_YUY2";
		case DXGI_FORMAT_Y210:
			return "DXGI_FORMAT_Y210";
		case DXGI_FORMAT_Y216:
			return "DXGI_FORMAT_Y216";
		case DXGI_FORMAT_NV11:
			return "DXGI_FORMAT_NV11";
		case DXGI_FORMAT_AI44:
			return "DXGI_FORMAT_AI44";
		case DXGI_FORMAT_IA44:
			return "DXGI_FORMAT_IA44";
		case DXGI_FORMAT_P8:
			return "DXGI_FORMAT_P8";
		case DXGI_FORMAT_A8P8:
			return "DXGI_FORMAT_A8P8";
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			return "DXGI_FORMAT_B4G4R4A4_UNORM";
		case DXGI_FORMAT_P208:
			return "DXGI_FORMAT_P208";
		case DXGI_FORMAT_V208:
			return "DXGI_FORMAT_V208";
		case DXGI_FORMAT_V408:
			return "DXGI_FORMAT_V408";
		case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE:
			return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE";
		case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE:
			return "DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE";
		case DXGI_FORMAT_A4B4G4R4_UNORM:
			return "DXGI_FORMAT_A4B4G4R4_UNORM";
		case DXGI_FORMAT_FORCE_UINT:
			return "DXGI_FORMAT_FORCE_UINT";
		default:
			return "";
	}
}