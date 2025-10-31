# ReShade shaders
 
**AMD FidelityFX Contrast Adaptive Sharpening**  
An accurate port of AMD FFX CAS.  
Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

**Modified AMD FidelityFX Contrast Adaptive Sharpening**  
Modified AMD FFX CAS.  
Rescaled the effect of sharpening, now sharpness = 0 means no sharpening and sharpness = 1 is same as before.  
Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/cas/ffx_cas.h

**AMD FidelityFX Robust Contrast Adaptive Sharpening**  
An accurate port of AMD FFX RCAS.  
Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

**AMD FidelityFX Linear Film Grain Aplicator**  
An acucurate port of AMD FFX LFGA using blue noise as the grain source.  
Note: The required blue noise texture (LFGANoise.png) is in the Texures folder.  
Ported from: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/54fbaafdc34716811751bea5032700e78f5a0f33/sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h

**Color Management System (CMS)**  
Color management system provided trough generated 3D LUT using a fast trilinear interpolation or a high quality tetrahidral interpolation.  
Note: To generate required 3D LUT for your game and display use provided CMSLUTGenerator.

**Draw Overlay**  
Draw an image overlay. Most usefull for crosshairs.  
Note: Required overlay.png texture should be of the same size as a game resolution (back buffer size). The texture background should be transparent.

## Addons

**AnyUpscale**  
A spatial upscaler.

**UpgradeSwapEffect**  
Upgrades DXGI swapchain to flip model and enable tearing (modern borderless window).  
Notes: Use only on DX10+ games.

**UpgradeRenderTargets**  
Upgrades render targets to higher precision formats.

**AccurateFPSLimiter**  
An accurate FPS limiter.

**BioshockGraphicalUpgrade**  
Graphical upgrade for Bioshock. See Addons/BioshockGraphicalUpgrade for details.

**FarCry2GraphicalUpgrade**  
Graphical upgrade for Far Cry 2. See Addons/FarCry2GraphicalUpgrade for details.

**BFBC2GraphicalUpgrade**  
Graphical upgrade for Battlefield: Bad Company 2. See Addons/BFBC2GraphicalUpgrade for details.

**GrisGraphicalUpgrade**  
Graphical upgrade for Gris. See Addons/GrisGraphicalUpgrade for details.

**DiscoElysiumGraphicalUpgrade**  
Graphical upgrade for Disco Elysium. See Addons/DiscoElysiumGraphicalUpgrade for details.

**HadesGraphicalUpgrade**  
Graphical upgrade for Hades. See Addons/HadesGraphicalUpgrade for details.

**HollowKnightGraphicalUpgrade**  
Graphical upgrade for Hollow Knight. See Addons/HollowKnightGraphicalUpgrade for details.