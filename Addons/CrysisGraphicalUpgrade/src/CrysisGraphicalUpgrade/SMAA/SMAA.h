#pragma once

#include "AreaTex.h"
#include "SearchTex.h"

// We need to pass this to all SMAA shaders on resolution change.
class SMAA_rt_metrics
{
public:

	void set(float width, float height)
	{
		val_str = std::format("float4({},{},{},{})", 1.0f / width, 1.0f / height, width, height);
		defines[0] = { "SMAA_RT_METRICS", val_str.c_str() };
	}

	const D3D_SHADER_MACRO* get() const noexcept
	{
		return defines;
	}

private:

	std::string val_str;
	D3D_SHADER_MACRO defines[2] = {};
};

constexpr float g_smaa_clear_color[4] = {};