# AnyUpscale

AnyUpscale is a spatial upscaler that has direct access to game's resources, its own swapchain and window with disabled input. All this results in low overhead, no added input lag and ability to directly change game's resources if necessary.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **AnyUpscaleDX11.addon32** or **AnyUpscaleDX11.addon64** and **AnyUpscale** folder in the game's folder where ReShade is installed.

Notes:
- Works with DirectX 11 only.
- The game should run in windowed or borderless windowed mode.
- You should set **Source width** and **Source height** to your game's resolution, afterwards should restart the game.
- If you lose input to the game, make the game's window active again.
- Some games don't handle mouse cursor properly when in widowed mode.
- If you are unable to use the ReShade configuration overlay to configure **AnyUpscale** you can configure it in the **ReShade.ini** under the **[AnyUpscale]** section.
- If you encounter problems check the **ReShade.log**.

## Upscalers

**SGSR1**  
https://www.qualcomm.com/news/onq/2023/04/introducing-snapdragon-game-super-resolution

**AMD FFX FSR1 EASU**  
https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-spatial

**Catmull-Rom**  
A fast aproximation of Catmull-Rom in 5 samples by omitting edge samples.

**Power of Garamond**  
A generic orthogonal (2 pass 1D) sampling that uses Power of Garamond windowed Sinc kernel function.  
Power of Garamond window can closely aproximate any other window (Hann, Cosine, etc.). Beyond that it can be adjusted for the best achivable results.  
Using intergers for its parameters (n and m) should give you the best performance. The defaults are n = 2 and m = 1, which is the exact Welch window.

Sharpening filter **AMD FFX RCAS**  
Some of upscalers use sharpening filter on top.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add AnyUpscale project to Examples solution, then build the project.
- Depends on [MinHook](https://github.com/TsudaKageyu/minhook) (libMinHook.x86.lib) and (libMinHook.x64.lib). You can also get precompiled lib from the official github (MinHook_xxx_lib.zip). You can just copy libMinHook.x86.lib and libMinHook.x64.lib into the project folder.