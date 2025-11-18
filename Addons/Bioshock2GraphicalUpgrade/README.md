# Bioshock2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- XeGTAO ambient occlusion.
- SMAA anti aliasing.
- 16x anisotropic filtering.
- Sharpening filter (Modified AMD FFX CAS).
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Accurate FPS limiter.
- Upgraded render targets (greatly reduces color banding).
- Greatly reduced input lag.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **Bioshock2GraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the game folder where **Bioshock2.exe** is (ReShade should be installed in the same folder).
- Note that it works (should be used) with DirectX 10 only.
- Note that it's tested on single player only.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add Bioshock2GraphicalUpgrade project to Examples solution, then build the project.