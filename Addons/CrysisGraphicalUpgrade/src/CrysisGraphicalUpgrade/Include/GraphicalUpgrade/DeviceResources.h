#pragma once

#include "Common.h"
#include "GraphicalUpgradePath.h"
#include "Ensure.h"
#include "Helpers.h"

// Defaults
//

__forceinline constexpr D3D10_SAMPLER_DESC default_D3D10_SAMPLER_DESC()
{
	return D3D10_SAMPLER_DESC {
		.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 16u,
		.ComparisonFunc = D3D10_COMPARISON_NEVER,
		.BorderColor = { 0.0f, 0.0f, 0.0f, 0.0f },
		.MinLOD = 0.0f,
		.MaxLOD = D3D10_FLOAT32_MAX
	};
}

__forceinline constexpr D3D10_DEPTH_STENCIL_DESC default_D3D10_DEPTH_STENCIL_DESC()
{
	return D3D10_DEPTH_STENCIL_DESC {
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
}

__forceinline constexpr D3D10_BLEND_DESC default_D3D10_BLEND_DESC()
{
	return D3D10_BLEND_DESC {
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
}

//

struct Managed_resources
{
	std::unordered_map<uint32_t, Com_ptr<ID3D10ShaderResourceView>> shader_resource_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D10RenderTargetView>> render_target_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D10DepthStencilView>> depth_stencil_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D10Resource>> resources;
	std::unordered_map<uint32_t, Com_ptr<ID3D10Buffer>> buffers;
	std::unordered_map<uint32_t, Com_ptr<ID3D10Texture1D>> textures_1d;
	std::unordered_map<uint32_t, Com_ptr<ID3D10Texture2D>> textures_2d;
	std::unordered_map<uint32_t, Com_ptr<ID3D10Texture3D>> textures_3d;
	std::unordered_map<uint32_t, Com_ptr<ID3D10InputLayout>> input_layouts;
	std::unordered_map<uint32_t, Com_ptr<ID3D10RasterizerState>> rasterizers;
	std::unordered_map<uint32_t, Com_ptr<ID3D10SamplerState>> samplers;
	std::unordered_map<uint32_t, Com_ptr<ID3D10BlendState>> blends;
	std::unordered_map<uint32_t, Com_ptr<ID3D10DepthStencilState>> depth_stencils;
	std::unordered_map<uint32_t, Com_ptr<ID3D10VertexShader>> vertex_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D10PixelShader>> pixel_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D10GeometryShader>> geometry_shaders;

	void clear()
	{
		shader_resource_views.clear();
		render_target_views.clear();
		depth_stencil_views.clear();
		resources.clear();
		buffers.clear();
		textures_1d.clear();
		textures_2d.clear();
		textures_3d.clear();
		input_layouts.clear();
		rasterizers.clear();
		samplers.clear();
		blends.clear();
		depth_stencils.clear();
		vertex_shaders.clear();
		pixel_shaders.clear();
		geometry_shaders.clear();
	}
};

inline void reset_com_array(auto& array)
{
	for (auto*& ptr : array) {
		if (ptr) {
			ptr->Release();
			ptr = nullptr;
		}
	}
}

inline void release_com_array(auto& array)
{
	for (auto*& ptr : array) {
		if (ptr) {
			ptr->Release();
		}
	}
}

inline void compile_shader(ID3DBlob** code, const wchar_t* file, const char* target, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
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

inline void create_vertex_shader(ID3D10Device* device, ID3D10VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "vs_4_1", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), vs), >= 0);
}

inline void create_pixel_shader(ID3D10Device* device, ID3D10PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "ps_4_1", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), ps), >= 0);
}

inline void create_constant_buffer(ID3D10Device* device, UINT size, ID3D10Buffer** cb)
{
	D3D10_BUFFER_DESC desc = {};
	desc.ByteWidth = size;
	desc.Usage = D3D10_USAGE_DYNAMIC;
	desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	ensure(device->CreateBuffer(&desc, nullptr, cb), >= 0);
}

inline void update_constant_buffer(ID3D10Buffer* cb, void* data, size_t size)
{
	void* mapped;
	ensure(cb->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped), >= 0);
	std::memcpy(mapped, data, size);
	cb->Unmap();
}

inline void create_sampler_point_clamp(ID3D10Device* device, ID3D10SamplerState** smp)
{
	auto desc = default_D3D10_SAMPLER_DESC();
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}

inline void create_sampler_linear_clamp(ID3D10Device* device, ID3D10SamplerState** smp)
{
	auto desc = default_D3D10_SAMPLER_DESC();
	desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}

