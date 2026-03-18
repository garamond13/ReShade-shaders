# BioshockGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- XeGTAO ambient occlusion.
- SMAA anti aliasing.
- Replaced bloom.
- 16x anisotropic filtering.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal blue noise film grain (AMD FFX LFGA).
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Accurate FPS limiter.
- Improved tonemapping.
- Upgraded render targets (greatly reduces color banding).
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **BioshockGraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the game folder where **Bioshock.exe** is (ReShade should be installed in the same folder).
- Note that it works (should be used) with DirectX 10 only.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add BioshockGraphicalUpgrade project to Examples solution, then build the project.