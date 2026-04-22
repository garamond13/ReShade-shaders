# DishonoredDOTOGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Replaced ambient occlusion (SSAO) with XeGTAO.
- Optionally replace TAA with DLSS (DLAA).
- Adjustable bloom intensity.
- Adjustable vigenette strenghth.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal blue noise film grain (AMD FFX LFGA).
- Adjustable Tone Response Curve (TRC) (sRGB, gamma).
- Optionally disable lens distortion.
- Optionally disable lens dirt.
- Upgraded render targets.
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **DishonoredDOTOGraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Dishonored_DO.exe** is (ReShade should be installed in the same folder).

## Notes

 - "Options->Visuals->Advanced Settings->Adaptive Resolution" should be set to "Off". Not supported at moment.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add DishonoredDOTOGraphicalUpgrade project to Examples solution, then build the project.
