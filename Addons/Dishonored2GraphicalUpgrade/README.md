# Dishonored2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Replaced ambient occlusion (SSAO) with Visibility Bitmask - GTAO.
- Replaced TAA.
- Optionally replace TAA with DLSS (DLAA).
- Improved bloom.
- Adjustable bloom intensity.
- Adjustable vigenette strenghth.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal film grain (AMD FFX LFGA).
- Optionally disable lens distortion.
- Optionally disable lens dirt.
- Upgraded render targets.
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **Dishonored2GraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Dishonored2.exe** is (ReShade should be installed in the same folder).

## Notes

 - "Options->Visuals->Advanced Settings->Adaptive Resolution" should be set to "Off". Not supported at moment.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add Dishonored2GraphicalUpgrade project to Examples solution, then build the project.
