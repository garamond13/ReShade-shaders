#pragma once

#include "Common.h"
#include "GraphicalUpgradePath.h"
#include "Ensure.h"
#include "Helpers.h"

struct Managed_resources
{
	std::unordered_map<uint32_t, Com_ptr<ID3D11ShaderResourceView>> shader_resource_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D11RenderTargetView>> render_target_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D11UnorderedAccessView>> unordered_access_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D11DepthStencilView>> depth_stencil_views;
	std::unordered_map<uint32_t, Com_ptr<ID3D11Resource>> resources;
	std::unordered_map<uint32_t, Com_ptr<ID3D11Buffer>> buffers;
	std::unordered_map<uint32_t, Com_ptr<ID3D11Texture1D>> textures_1d;
	std::unordered_map<uint32_t, Com_ptr<ID3D11Texture2D>> textures_2d;
	std::unordered_map<uint32_t, Com_ptr<ID3D11Texture3D>> textures_3d;
	std::unordered_map<uint32_t, Com_ptr<ID3D11InputLayout>> input_layouts;
	std::unordered_map<uint32_t, Com_ptr<ID3D11RasterizerState>> rasterizers;
	std::unordered_map<uint32_t, Com_ptr<ID3D11SamplerState>> samplers;
	std::unordered_map<uint32_t, Com_ptr<ID3D11BlendState>> blends;
	std::unordered_map<uint32_t, Com_ptr<ID3D11DepthStencilState>> depth_stencils;
	std::unordered_map<uint32_t, Com_ptr<ID3D11VertexShader>> vertex_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D11ComputeShader>> compute_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D11PixelShader>> pixel_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D11DomainShader>> domain_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D11GeometryShader>> geometry_shaders;
	std::unordered_map<uint32_t, Com_ptr<ID3D11HullShader>> hull_shaders;

	void clear()
	{
		shader_resource_views.clear();
		render_target_views.clear();
		unordered_access_views.clear();
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
		compute_shaders.clear();
		pixel_shaders.clear();
		domain_shaders.clear();
		geometry_shaders.clear();
		hull_shaders.clear();
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

inline void create_vertex_shader(ID3D11Device* device, ID3D11VertexShader** vs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "vs_5_0", entry_point, defines);
	ensure(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vs), >= 0);
}

inline void create_pixel_shader(ID3D11Device* device, ID3D11PixelShader** ps, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "ps_5_0", entry_point, defines);
	ensure(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, ps), >= 0);
}

inline void create_compute_shader(ID3D11Device* device, ID3D11ComputeShader** cs, const wchar_t* file, const char* entry_point = "main", const D3D_SHADER_MACRO* defines = nullptr)
{
	Com_ptr<ID3DBlob> code;
	compile_shader(code.put(), file, "cs_5_0", entry_point, defines);
	ensure(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, cs), >= 0);
}

inline void create_constant_buffer(ID3D11Device* device, UINT size, ID3D11Buffer** cb)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ensure(device->CreateBuffer(&desc, nullptr, cb), >= 0);
}

inline void update_constant_buffer(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const void* data, size_t size)
{
	D3D11_MAPPED_SUBRESOURCE mapped_subresource;
	ensure(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource), >= 0);
	std::memcpy(mapped_subresource.pData, data, size);
	ctx->Unmap(cb, 0);
}

inline void create_sampler_point_clamp(ID3D11Device* device, ID3D11SamplerState** smp)
{
	CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}

inline void create_sampler_linear_clamp(ID3D11Device* device, ID3D11SamplerState** smp)
{
	CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
	ensure(device->CreateSamplerState(&desc, smp), >= 0);
}
