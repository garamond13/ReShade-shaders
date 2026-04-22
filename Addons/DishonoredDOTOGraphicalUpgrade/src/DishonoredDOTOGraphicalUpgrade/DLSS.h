#pragma once

#include "Include/GraphicalUpgrade.h"
#include "DLSS/include/nvsdk_ngx_helpers.h"

// We only support/provide DLAA.
// Preconfigured for Dishonored: Death of the Outsider.

// Exposed to user.
enum DLSS_PRESET_
{
	DLSS_PRESET_E,
	DLSS_PRESET_F,
	DLSS_PRESET_K
};
typedef int DLSS_PRESET;

// DLSS can be initialized per device,
// regardless we want only one instance.
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
		if (is_initialized) {
			return;
		}
		auto result = NVSDK_NGX_D3D11_Init_with_ProjectID("bae402ad-83b8-4225-89b6-cee2ccabfe05", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".", device);
		if (NVSDK_NGX_SUCCEED(result)) {
			log_info("DLSS: Successfully initialized.");
		}
		else {
			log_error("DLSS: Faild to initialize!");
		}
	}

	void create_feature(ID3D11DeviceContext* ctx, uint32_t width, uint32_t height, DLSS_PRESET preset)
	{
		// Minimum supported resolution by DLSS.
		assert(width >= 32 && height >= 32);

		// Release old resources first.
		release_feature();

		// Pick an acctual preset.
		NVSDK_NGX_DLSS_Hint_Render_Preset dlss_hint_renderer_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
		switch (preset) {
			case DLSS_PRESET_E:
				dlss_hint_renderer_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_E;
				break;
			case DLSS_PRESET_F:
				dlss_hint_renderer_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_F;
				break;
			case DLSS_PRESET_K:
				dlss_hint_renderer_preset = NVSDK_NGX_DLSS_Hint_Render_Preset_K;
				break;
		}
		assert(dlss_hint_renderer_preset != NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
		
		auto result = NVSDK_NGX_D3D11_AllocateParameters(&feature);
		if (NVSDK_NGX_SUCCEED(result)) {
			NVSDK_NGX_Parameter_SetUI(feature, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, dlss_hint_renderer_preset);

			int flags = NVSDK_NGX_DLSS_Feature_Flags_None;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
			flags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

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

	void draw(ID3D11DeviceContext* ctx, NVSDK_NGX_D3D11_DLSS_Eval_Params& eval_params) const
	{
		auto result = NGX_D3D11_EVALUATE_DLSS_EXT(ctx, handle, feature, &eval_params);
		assert(NVSDK_NGX_SUCCEED(result));
	}

	void release_feature()
	{
		if (handle) {
			auto result = NVSDK_NGX_D3D11_ReleaseFeature(handle);
			assert(NVSDK_NGX_SUCCEED(result));
			handle = nullptr;
		}
		if (feature) {
			auto result = NVSDK_NGX_D3D11_DestroyParameters(feature);
			assert(NVSDK_NGX_SUCCEED(result));
			feature = nullptr;
		}
	}

	void shutdown()
	{
		if (!is_initialized) {
			return;
		}
		release_feature();
		auto result = NVSDK_NGX_D3D11_Shutdown1(nullptr);
		assert(NVSDK_NGX_SUCCEED(result));
	}

private:

	bool is_initialized = false;
	NVSDK_NGX_Handle* handle = nullptr;
	NVSDK_NGX_Parameter* feature = nullptr;
};