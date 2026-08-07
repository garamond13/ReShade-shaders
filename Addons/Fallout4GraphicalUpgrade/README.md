# Fallout4GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Optionally replace TAA with DLSS (DLAA only).
- Replaced ambient occlusion (SSAO) with XeGTAO.
- Replaced bloom.
- Sharpening filter (Modified AMD FFX CAS).
- Improved tonemapping.
- Fixed/improved some shaders.
- Upgraded render targets.
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.8.0 or newer with full add-on support.
- Copy **Fallout4GraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Fallout4.exe** is (ReShade should be installed in the same folder).

## Notes

- DLSS may crash the game during loading screen on some DLSS presets.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add Fallout4GraphicalUpgrade project to Examples solution, then build the project.
