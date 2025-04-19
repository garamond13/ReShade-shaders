# ReShade shaders
 
**AMD FidelityFX Contrast Adaptive Sharpening**  
An accurate port of AMD FFX CAS.  
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

**UpgradeSwapEffect addon**
Upgrades DXGI swapchain swap effect to DXGI_SWAP_EFFECT_FLIP_DISCARD.  
Note: Use only with DX10 or DX11 games.