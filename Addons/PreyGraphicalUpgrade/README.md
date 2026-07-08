# PreyGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Replaced ambient occlusion (Screen Space Directional Occlusion->Full Resolution) with GTAO.
- Optionally replace TAA (SMAA 2TX) with DLSS (DLAA).
- Adjustable vigenette strenghth.
- Optionally disable motion blur.
- Fixed pixelated shadows.
- Force 16x anisotropic filtering.
- Upgraded render targets.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **PreyGraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Prey.exe** is (ReShade should be installed in the same folder).

## Notes

 - "OPTIONS->VIDEO->ADVANCED SETTINGS->Adaptive Resolution" should be set to "OFF".

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add PreyGraphicalUpgrade project to Examples solution, then build the project.
