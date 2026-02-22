#pragma once
/*
* Copyright (c) 2021-2024 NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software, related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is strictly
* prohibited.
*
* TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
* SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
* LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
* BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
* INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGES.
*/


#ifndef NVSDK_NGX_DEFS_DLSSG_H
#define NVSDK_NGX_DEFS_DLSSG_H

#include <stddef.h> // for size_t
#include <stdint.h> // for uint64_t

#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_params.h"

#define NVSDK_NGX_Parameter_FrameGeneration_Available                "FrameGeneration.Available"
#define NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult        "FrameGeneration.FeatureInitResult"
#define NVSDK_NGX_Parameter_FrameGeneration_MinDriverVersionMajor    "FrameGeneration.MinDriverVersionMajor"
#define NVSDK_NGX_Parameter_FrameGeneration_MinDriverVersionMinor    "FrameGeneration.MinDriverVersionMinor"
#define NVSDK_NGX_Parameter_FrameGeneration_NeedsUpdatedDriver       "FrameGeneration.NeedsUpdatedDriver"

#define NVSDK_NGX_Parameter_FrameInterpolation_Available                "FrameInterpolation.Available"
#define NVSDK_NGX_Parameter_FrameInterpolation_FeatureInitResult        "FrameInterpolation.FeatureInitResult"
#define NVSDK_NGX_Parameter_FrameInterpolation_MinDriverVersionMajor    "FrameInterpolation.MinDriverVersionMajor"
#define NVSDK_NGX_Parameter_FrameInterpolation_MinDriverVersionMinor    "FrameInterpolation.MinDriverVersionMinor"
#define NVSDK_NGX_Parameter_FrameInterpolation_NeedsUpdatedDriver       "FrameInterpolation.NeedsUpdatedDriver"

#define NVSDK_NGX_DLSSG_Parameter_BackbufferFormat           "DLSSG.BackbufferFormat"

#define NVSDK_NGX_DLSSG_Parameter_Backbuffer                            "DLSSG.Backbuffer"
#define NVSDK_NGX_DLSSG_Parameter_MVecs                                 "DLSSG.MVecs"
#define NVSDK_NGX_DLSSG_Parameter_Depth                                 "DLSSG.Depth"
#define NVSDK_NGX_DLSSG_Parameter_HUDLess                               "DLSSG.HUDLess"
#define NVSDK_NGX_DLSSG_Parameter_UI                                    "DLSSG.UI"
#define NVSDK_NGX_DLSSG_Parameter_NoPostProcessingColor                 "DLSSG.NoPostProcessingColor"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField          "DLSSG.BidirectionalDistortionField"
#define NVSDK_NGX_Parameter_OutputInterpolated                          "DLSSG.OutputInterpolated"
#define NVSDK_NGX_Parameter_OutputReal                                  "DLSSG.OutputReal"

// DLFG-specific width/height parameters, to avoid name conflicts with other
// features. If set in CreateFeature, these parameters will be used instead of
// the generic NVSDK_NGX_Parameter{Width,Height}
#define NVSDK_NGX_DLSSG_Parameter_Width     "DLSSG.Width"
#define NVSDK_NGX_DLSSG_Parameter_Height    "DLSSG.Height"

// Output resource which allows the snippet to hint to the host application that
// the interpolated frame(s) should not be shown this frame. This resource must
// be created by the application with a minimum size of 4 bytes, and will be filled
// by the snippet: if interpolation should be disabled for this frame, boolean true
// will be written to the first byte of the buffer.
#define NVSDK_NGX_DLSSG_Parameter_OutputDisableInterpolation "DLSSG.OutputDisableInterpolation"

//! IMPORTANT: All matrices should NOT contain temporal AA jitter offset
//! Specifies matrix transformation from the camera view to the clip space.
//! Float4x4 as void*
#define NVSDK_NGX_DLSSG_Parameter_CameraViewToClip              "DLSSG.CameraViewToClip"
//! Float4x4 as void*
#define NVSDK_NGX_DLSSG_Parameter_ClipToCameraView              "DLSSG.ClipToCameraView"
//! Float4x4 as void*
#define NVSDK_NGX_DLSSG_Parameter_ClipToLensClip                "DLSSG.ClipToLensClip"
//! Float4x4 as void*
#define NVSDK_NGX_DLSSG_Parameter_ClipToPrevClip                "DLSSG.ClipToPrevClip"
//! Float4x4 as void*
#define NVSDK_NGX_DLSSG_Parameter_PrevClipToClip                "DLSSG.PrevClipToClip"

//! Specifies clip space jitter offset
#define NVSDK_NGX_DLSSG_Parameter_JitterOffsetX              "DLSSG.JitterOffsetX"
#define NVSDK_NGX_DLSSG_Parameter_JitterOffsetY              "DLSSG.JitterOffsetY"

//! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
#define NVSDK_NGX_DLSSG_Parameter_MvecScaleX                 "DLSSG.MvecScaleX"
#define NVSDK_NGX_DLSSG_Parameter_MvecScaleY                 "DLSSG.MvecScaleY"

//! Specifies camera pinhole offset.
#define NVSDK_NGX_DLSSG_Parameter_CameraPinholeOffsetX       "DLSSG.CameraPinholeOffsetX"
#define NVSDK_NGX_DLSSG_Parameter_CameraPinholeOffsetY       "DLSSG.CameraPinholeOffsetY"

//! Specifies camera position in world space.
#define NVSDK_NGX_DLSSG_Parameter_CameraPosX                 "DLSSG.CameraPosX"
#define NVSDK_NGX_DLSSG_Parameter_CameraPosY                 "DLSSG.CameraPosY"
#define NVSDK_NGX_DLSSG_Parameter_CameraPosZ                 "DLSSG.CameraPosZ"

//! Specifies camera up vector in world space.
#define NVSDK_NGX_DLSSG_Parameter_CameraUpX                  "DLSSG.CameraUpX"
#define NVSDK_NGX_DLSSG_Parameter_CameraUpY                  "DLSSG.CameraUpY"
#define NVSDK_NGX_DLSSG_Parameter_CameraUpZ                  "DLSSG.CameraUpZ"

//! Specifies camera right vector in world space.
#define NVSDK_NGX_DLSSG_Parameter_CameraRightX               "DLSSG.CameraRightX"
#define NVSDK_NGX_DLSSG_Parameter_CameraRightY               "DLSSG.CameraRightY"
#define NVSDK_NGX_DLSSG_Parameter_CameraRightZ               "DLSSG.CameraRightZ"

//! Specifies camera forward vector in world space.
#define NVSDK_NGX_DLSSG_Parameter_CameraFwdX                 "DLSSG.CameraFwdX"
#define NVSDK_NGX_DLSSG_Parameter_CameraFwdY                 "DLSSG.CameraFwdY"
#define NVSDK_NGX_DLSSG_Parameter_CameraFwdZ                 "DLSSG.CameraFwdZ"

//! Specifies camera near view plane distance.
#define NVSDK_NGX_DLSSG_Parameter_CameraNear                 "DLSSG.CameraNear"

//! Specifies camera far view plane distance.
#define NVSDK_NGX_DLSSG_Parameter_CameraFar                  "DLSSG.CameraFar"

//! Specifies camera field of view in radians.
#define NVSDK_NGX_DLSSG_Parameter_CameraFOV                  "DLSSG.CameraFOV"

//! Specifies camera aspect ratio defined as view space width divided by height.
#define NVSDK_NGX_DLSSG_Parameter_CameraAspectRatio          "DLSSG.CameraAspectRatio"

//! Specifies if tagged color buffers are full HDR (rendering to an HDR monitor) or not
#define NVSDK_NGX_DLSSG_Parameter_ColorBuffersHDR            "DLSSG.ColorBuffersHDR"

//! Specifies if depth values are inverted (value closer to the camera is higher) or not.
#define NVSDK_NGX_DLSSG_Parameter_DepthInverted              "DLSSG.DepthInverted"

//! Specifies if camera motion is included in the MVec buffer.
#define NVSDK_NGX_DLSSG_Parameter_CameraMotionIncluded       "DLSSG.CameraMotionIncluded"

//! Specifies if motion vectors include jitter data.

//! Specifies if previous frame has no connection to the current one (motion vectors are invalid)
#define NVSDK_NGX_DLSSG_Parameter_Reset                      "DLSSG.Reset"

//! Specifies if Automode overrode the input Reset flag to true.
#define NVSDK_NGX_DLSSG_Parameter_AutomodeOverrideReset      "DLSSG.AutomodeOverrideReset"

//! Specifies if application is not currently rendering game frames (paused in menu, playing video cut-scenes)
#define NVSDK_NGX_DLSSG_Parameter_NotRenderingGameFrames     "DLSSG.NotRenderingGameFrames"

//! Specifies if orthographic projection is used or not.
#define NVSDK_NGX_DLSSG_Parameter_OrthoProjection            "DLSSG.OrthoProjection"

#define NVSDK_NGX_Parameter_DLSSGGetCurrentSettingsCallback  "DLSSG.GetCurrentSettingsCallback"
#define NVSDK_NGX_Parameter_DLSSGEstimateVRAMCallback        "DLSSG.EstimateVRAMCallback"

#define NVSDK_NGX_Parameter_DLSSGMustCallEval                "DLSSG.MustCallEval"
#define NVSDK_NGX_Parameter_DLSSGBurstCaptureRunning         "DLSSG.BurstCaptureRunning"

#define NVSDK_NGX_Parameter_DLSSGInvertXAxis          "DLSSG.InvertXAxis"
#define NVSDK_NGX_Parameter_DLSSGInvertYAxis          "DLSSG.InvertYAxis"

#define NVSDK_NGX_DLSSG_Parameter_UserDebugText       "DLSSG.UserDebugText"

#define NVSDK_NGX_DLSSG_Parameter_MvecInvalidValue   "DLSSG.MvecInvalidValue"
#define NVSDK_NGX_DLSSG_Parameter_MvecDilated        "DLSSG.MvecDilated"
#define NVSDK_NGX_DLSSG_Parameter_MvecJittered       "DLSSG.MvecJittered"

#define NVSDK_NGX_DLSSG_Parameter_MenuDetectionEnabled "DLSSG.MenuDetectionEnabled"

//! If true, allow the snippet to create internal resources on a separate thread, to avoid hitches during feature creation (default false)
#define NVSDK_NGX_DLSSG_Parameter_AsyncCreateEnabled "DLSSG.AsyncCreateEnabled"

//! The DLSS Frame Generation algorithm processes all depth in a "linearized" depth space using the below formulation.
//! 
//!     if NVSDK_NGX_DLSSG_Parameter_DepthInverted is false:    `lin_depth = 1 / (1 - depth)`
//!     if NVSDK_NGX_DLSSG_Parameter_DepthInverted is true:     `lin_depth = 1 / depth`
//!
//! This enables the algorithm to reliably threshold and characterize depth interactions.
//! 
//! The Frame Generation algorithm is tuned such that most game integrations do not need to modify these values.
//! However, the below parameters provide mechanisms to control the shape of the internal depth processing space.
//! Typically useful when input raw depth units are compressed into a small dynamic range. The scale modifier
//! NVSDK_NGX_DLSSG_Parameter_LinearizedDepth_Scale is a good place to start that won't require more changes.
//! 
//! Note: the input depth texture must be consistent with the input camera transformation matrices, e.g. clip2prevclip.

//! Optional scalar on the linearized depth before algorithm processing.
//!
//! Defaults to 1.0f. A value of 0.1f can help with a compressed input depth range.
#define NVSDK_NGX_DLSSG_Parameter_LinearizedDepth_Scale "DLSSG.LinearizedDepth_Scale"

//! Optional heuristic that defines the z-distance to the depth plane that artificially partitions near from far objects.
//! DLSS Frame Generation uses this plane to restrict far plane object motion from scattering into near plane objects.
//!
//! Defaults to 600.0f. A value of 4000.0f can help with a compressed input depth range.
#define NVSDK_NGX_DLSSG_Parameter_LinearizedDepth_NearFarPartition "DLSSG.LinearizedDepth_NearFarPartition"

//! Optional heuristic that specifies the minimum linearized depth difference between two objects in screen-space.
//! DLSS Frame Generation uses this heuristic to understand contiguous depth surfaces.
//!
//! Defaults to 40.0f.
#define NVSDK_NGX_DLSSG_Parameter_MinRelativeLinearDepthObjectSeparation "DLSSG.MinRelativeLinearDepthObjectSeparation"

//! Texture Subrect Definitions

#define NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectBaseX  "DLSSG.BackbufferSubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectBaseY  "DLSSG.BackbufferSubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectWidth  "DLSSG.BackbufferSubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectHeight "DLSSG.BackbufferSubrectHeight"

#define NVSDK_NGX_DLSSG_Parameter_MVecsSubrectBaseX "DLSSG.MVecsSubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_MVecsSubrectBaseY "DLSSG.MVecsSubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_MVecsSubrectWidth "DLSSG.MVecsSubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_MVecsSubrectHeight "DLSSG.MVecsSubrectHeight"

#define NVSDK_NGX_DLSSG_Parameter_DepthSubrectBaseX "DLSSG.DepthSubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_DepthSubrectBaseY "DLSSG.DepthSubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_DepthSubrectWidth "DLSSG.DepthSubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_DepthSubrectHeight "DLSSG.DepthSubrectHeight"

#define NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectBaseX "DLSSG.HUDLessSubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectBaseY "DLSSG.HUDLessSubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectWidth "DLSSG.HUDLessSubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectHeight "DLSSG.HUDLessSubrectHeight"

#define NVSDK_NGX_DLSSG_Parameter_UISubrectBaseX "DLSSG.UISubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_UISubrectBaseY "DLSSG.UISubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_UISubrectWidth "DLSSG.UISubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_UISubrectHeight "DLSSG.UISubrectHeight"

#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectBaseX  "DLSSG.BidirectionalDistortionFieldSubrectBaseX"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectBaseY  "DLSSG.BidirectionalDistortionFieldSubrectBaseY"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectWidth  "DLSSG.BidirectionalDistortionFieldSubrectWidth"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectHeight "DLSSG.BidirectionalDistortionFieldSubrectHeight"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_IsLowPrecision "DLSSG.BidirectionalDistortionFieldLowPrecision.IsLowPrecision"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_Bias           "DLSSG.BidirectionalDistortionFieldLowPrecision.Bias"
#define NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_Scale          "DLSSG.BidirectionalDistortionFieldLowPrecision.Scale"

//! Specifies the internal resolution to be used (mainly for dynamic resolution)
#define NVSDK_NGX_DLSSG_Parameter_InternalWidth      "DLSSG.InternalWidth"
#define NVSDK_NGX_DLSSG_Parameter_InternalHeight     "DLSSG.InternalHeight"

//! If nonzero, we expect to use dynamic resolution (MVec and Depth res will change, possibly per frame)
#define NVSDK_NGX_DLSSG_Parameter_DynamicResolution  "DLSSG.DynamicResolution"

typedef enum NVSDK_NGX_DLSSG_EvalFlags
{
   // The default behaviour for evaluate is to write the interpolated pixels inside of the
   // backbuffer extent and uninterpolated pixels - outside of the extent. If this flag is
   // set, then only the inside of the backbuffer extent is updated, the outside of the extent
   // stays untouched.
   NVSDK_NGX_DLSSG_EvalFlags_UpdateOnlyInsideExtents = (1 << 0)
} NVSDK_NGX_DLSSG_EvalFlags;
#define NVSDK_NGX_DLSSG_Parameter_EvalFlags "DLSSG.EvalFlags"

// Callback function types
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_DLSSG_GetCurrentSettingsCallback)(const NVSDK_NGX_Handle* InHandle, NVSDK_NGX_Parameter* InParams);
typedef NVSDK_NGX_Result(NVSDK_CONV* PFN_NVSDK_NGX_DLSSG_EstimateVRAMCallback)(uint32_t mvecDepthWidth, uint32_t mvecDepthHeight,
    uint32_t colorWidth, uint32_t colorHeight,
    uint32_t colorBufferFormat,
    uint32_t mvecBufferFormat, uint32_t depthBufferFormat,
    uint32_t hudLessBufferFormat, uint32_t uiBufferFormat, size_t* estimatedVRAMInBytes);

// DLSS 4

// Output Capability Parameters (access via GetDeviceCapabilityParameters)
// The maximum number of generated frames supported by the feature (e.g. 3 for 4x)
// If 1 or not set, multiframe is not supported
#define NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax            "DLSSG.MultiFrameCountMax"

// InParameters
// uint32_t, Total of Number Frames expected to Interpolate for given Real-Frame Pair. For 4x, this is 3. For 2x, this is 1.
#define NVSDK_NGX_DLSSG_Parameter_MultiFrameCount               "DLSSG.MultiFrameCount"
// uint32_t, Current inner frame index. For 4x, this is 1, 2, and 3. For 2x, this is 1.
#define NVSDK_NGX_DLSSG_Parameter_MultiFrameIndex               "DLSSG.MultiFrameIndex"

//! Optional: uint64_t that increments by one for each fully rendered backbuffer frame such that
//!   Current Backbuffer ID == (Previous Backbuffer ID + 1). The number should increment even
//!   if FG is off, for example, when rendering a menu with FG temporarily disabled.
#define NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID            "DLSSG.BackbufferFrameID"


#endif // NVSDK_NGX_DEFS_DLSSG_H
