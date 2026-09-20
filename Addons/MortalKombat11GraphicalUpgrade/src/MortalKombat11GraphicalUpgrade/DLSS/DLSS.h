#pragma once

#include "Include/GraphicalUpgrade.h"
#include "DLSS/include/nvsdk_ngx_helpers.h"

// DLSS can be initialized per device,
// regardless we want only one instance.
class DLSS
{
public:
	
	static DLSS& get_instance()
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

	void init(ID3D11Device* device)
	{
		// Make sure we ony have one instance.
		shutdown();

		auto result = NVSDK_NGX_D3D11_Init_with_ProjectID("bae402ad-83b8-4225-89b6-cee2ccabfe05", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".", device);
		if (NVSDK_NGX_SUCCEED(result)) {
			log_info("DLSS: Successfully initialized.");
		}
		else {
			log_error("DLSS: Faild to initialize!");
		}
	}

	void create_feature(ID3D11DeviceContext* ctx, uint32_t width, uint32_t height, NVSDK_NGX_DLSS_Hint_Render_Preset preset, int flags)
	{
		// Minimum supported resolution by DLSS.
		assert(width >= 32 && height >= 32);

		// Release old resources first.
		release_feature();
		
		auto result = NVSDK_NGX_D3D11_AllocateParameters(&m_feature);
		if (NVSDK_NGX_SUCCEED(result)) {
			NVSDK_NGX_Parameter_SetUI(m_feature, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, preset);

			NVSDK_NGX_DLSS_Create_Params create_params = {};
			create_params.Feature.InTargetWidth = width;
			create_params.Feature.InTargetHeight = height;
			create_params.Feature.InWidth = width;
			create_params.Feature.InHeight = height;
			create_params.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
			create_params.InFeatureCreateFlags = flags;
			result = NGX_D3D11_CREATE_DLSS_EXT(ctx, &m_handle, m_feature, &create_params);
		}
		if (NVSDK_NGX_SUCCEED(result)) {
			log_info("DLSS: Successfully created feature.");
		}
		else {
			log_error("DLSS: Faild to create feature!");
		}
	}

	bool draw(ID3D11DeviceContext* ctx, NVSDK_NGX_D3D11_DLSS_Eval_Params& eval_params) const
	{
		auto result = NGX_D3D11_EVALUATE_DLSS_EXT(ctx, m_handle, m_feature, &eval_params);
		return NVSDK_NGX_SUCCEED(result);
	}

	void shutdown()
	{
		release_feature();
		NVSDK_NGX_D3D11_Shutdown1(nullptr);
	}

private:

	void release_feature()
	{
		if (m_handle) {
			auto result = NVSDK_NGX_D3D11_ReleaseFeature(m_handle);
			assert(NVSDK_NGX_SUCCEED(result));
			m_handle = nullptr;
		}
		if (m_feature) {
			auto result = NVSDK_NGX_D3D11_DestroyParameters(m_feature);
			assert(NVSDK_NGX_SUCCEED(result));
			m_feature = nullptr;
		}
	}

	NVSDK_NGX_Handle* m_handle = nullptr;
	NVSDK_NGX_Parameter* m_feature = nullptr;
};
