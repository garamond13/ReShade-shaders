#pragma once

#include "Common.h"
#include "Helpers.h"
#include "DLSS/include/nvsdk_ngx_helpers.h"

// We only support/provide DLAA.
// Preconfigured for Dishonored 2.

class DLSS
{
public:
	
	static DLSS& instance()
	{
		static DLSS instance;
		return instance;
	}

	// Delete copy/move.
	DLSS(const DLSS&) = delete;
	DLSS& operator=(const DLSS&) = delete;
	DLSS(DLSS&&) = delete;
	DLSS& operator=(DLSS&&) = delete;

private:

	DLSS() = default;
	~DLSS() = default;

public:

	void init(ID3D11Device* device) const
	{
		NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init_with_ProjectID("29a8baf9-f6f9-40a7-a6dd-ac1b651cc617", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".", device);
		
		if (NVSDK_NGX_SUCCEED(result)) {
			log_info("DLSS: Successfully initialized.");
		}
		else {
			log_error("DLSS: Faild to initialize!");
		}
	}

	void create_feature(ID3D11DeviceContext* ctx, uint32_t width, uint32_t height)
	{
		// Release old resources first.
		release_feature();
		
		this->width = width;
		this->height = height;

		auto result = NVSDK_NGX_D3D11_AllocateParameters(&feature);
		if (NVSDK_NGX_SUCCEED(result)) {
			NVSDK_NGX_Parameter_SetUI(feature, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_DLSS_Hint_Render_Preset_F);

			int flags = NVSDK_NGX_DLSS_Feature_Flags_None;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;

			NVSDK_NGX_DLSS_Create_Params create_params = {};
			create_params.Feature.InTargetWidth = width;
			create_params.Feature.InTargetHeight = height;
			create_params.Feature.InWidth = width;
			create_params.Feature.InHeight = height;
			create_params.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
			create_params.InFeatureCreateFlags = flags;
			result = NGX_D3D11_CREATE_DLSS_EXT(ctx, &handle, feature, &create_params);
		}

		if (NVSDK_NGX_SUCCEED(result)) {
			log_info("DLSS: Successfully created feature.");
		}
		else {
			log_error("DLSS: Faild to create feature!");
		}
	}

	void draw(ID3D11DeviceContext* ctx, ID3D11Resource* scene, ID3D11Resource* depth, ID3D11Resource* mvs, ID3D11Resource* exposure, float jitter_x, float jitter_y, ID3D11Resource* out) const
	{
		assert(scene);
		assert(depth);
		assert(mvs);
		assert(out);

		NVSDK_NGX_D3D11_DLSS_Eval_Params eval_params = {};
		eval_params.Feature.pInColor = scene;
		eval_params.Feature.pInOutput = out;
		eval_params.pInDepth = depth;
		eval_params.pInMotionVectors = mvs;
		eval_params.pInExposureTexture = exposure;
		eval_params.InRenderSubrectDimensions.Width = width;
		eval_params.InRenderSubrectDimensions.Height = height;
		eval_params.InJitterOffsetX = jitter_x;
		eval_params.InJitterOffsetY = jitter_y;
		NVSDK_NGX_Result result = NGX_D3D11_EVALUATE_DLSS_EXT(ctx, handle, feature, &eval_params);
		assert(NVSDK_NGX_SUCCEED(result));
	}

	void release_feature()
	{
		NVSDK_NGX_Result result;
		if (handle) {
			result = NVSDK_NGX_D3D11_ReleaseFeature(handle);
			assert(NVSDK_NGX_SUCCEED(result));
			handle = nullptr;
		}
		if (feature) {
			result = NVSDK_NGX_D3D11_DestroyParameters(feature);
			assert(NVSDK_NGX_SUCCEED(result));
			feature = nullptr;
		}
	}

	void shutdown()
	{
		release_feature();
		auto result = NVSDK_NGX_D3D11_Shutdown1(nullptr);
		assert(NVSDK_NGX_SUCCEED(result));
	}

private:

	int width = 0;
	int height = 0;
	NVSDK_NGX_Handle* handle = nullptr;
	NVSDK_NGX_Parameter* feature = nullptr;
};