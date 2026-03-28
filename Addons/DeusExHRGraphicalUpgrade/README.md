# DeusExHRGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Replaced SSAO ambient occlusion with XeGTAO.
- SMAA anti aliasing.
- Replaced bloom.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal blue noise film grain (AMD FFX LFGA).
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Accurate FPS limiter.
- Upgraded render targets (greatly reduces color banding).
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **DeusExHRGraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the game folder where **dxhr.exe** is (ReShade should be installed in the same folder).

## Notes

- DeusExHRGraphicalUpgrade is developed for, and tested only on the original release version (not Directors Cut (DC)).
- Works (should be used) with DirectX 11 only.
- SMAA is not replacing the game's anti aliasing, so you can enable/disable the game's anti aliasing on top of SMAA.
- Setting the in game option "OPTIONS->VIDEO->ADVANCED->ANTI-ALIASING" to "Edge AA" is recommended cause the game will use higher precision depth buffer.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add DeusExHRGraphicalUpgrade project to Examples solution, then build the project.