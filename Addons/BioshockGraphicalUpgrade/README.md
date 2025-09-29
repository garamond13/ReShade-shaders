# BioshockGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- SMAA anti aliasing.
- 16x anisotropic filtering.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal blue noise film grain (AMD FFX LFGA).
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Adjustable bloom intensity.
- Accurate FPS limiter.
- Upgraded render targets (greatly reduces color banding).
- 10bit color output.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **BioshockGraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the game folder where **Bioshock.exe** is (ReShade should be installed in the same folder).

## Compilation
- You can clone [ReShade](https://github.com/crosire/reshade) and add BioshockGraphicalUpgrade project to Examples solution, then build the project.
- Depends on [MinHook](https://github.com/TsudaKageyu/minhook) (libMinHook.x86.lib). You can also get precompiled lib from the official github (MinHook_xxx_lib.zip). You can just copy libMinHook.x86.lib into the project folder.