#ifndef __GRAPHICAL_UPGRADE_CB_HLSLI_H__
#define __GRAPHICAL_UPGRADE_CB_HLSLI_H__

#ifndef GRAPHICAL_UPGRADE_CB_SLOT
#define GRAPHICAL_UPGRADE_CB_SLOT 13
#endif

#ifdef __cplusplus
#include "GraphicalUpgrade/HLSLTypes.h"
#define GRAPHICAL_UPGRADE_CB_BEGIN struct alignas(16) Graphical_upgrade_cb {
#define GRAPHICAL_UPGRADE_CB_END };
#else // HLSL
#define GRAPHICAL_UPGRADE_CB_HLSL_SLOT(x,y) x##y
#define GRAPHICAL_UPGRADE_CB_BEGIN cbuffer Graphical_upgrade_cb : register(GRAPHICAL_UPGRADE_CB_HLSL_SLOT(b,GRAPHICAL_UPGRADE_CB_SLOT)) {
#define GRAPHICAL_UPGRADE_CB_END }
#endif

GRAPHICAL_UPGRADE_CB_BEGIN
int tex_noise_index;
GRAPHICAL_UPGRADE_CB_END

#endif // __GRAPHICAL_UPGRADE_CB_HLSLI_H__
