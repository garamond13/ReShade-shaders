# Dishonored2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Replaced ambient occlusion (SSAO) with XeGTAO.
- Replaced TAA.
- Optionally replace TAA with DLSS (DLAA).
- Imroved bloom.
- Adjustable bloom intensity.
- Adjustable vigenette strenght.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal blue noise film grain (AMD FFX LFGA).
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Optionally disable lens distortion.
- Optionally disable lens dirt.
- Upgraded render targets.
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.1 or newer with full add-on support.
- Copy **Dishonored2GraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Dishonored2.exe** is (ReShade should be installed in the same folder).

## Notes

 - "Options->Visuals->Advanced Settings->Adaptive Resolution" should be set to "Off". Not supported at moment.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add Dishonored2GraphicalUpgrade project to Examples solution, then build the project.