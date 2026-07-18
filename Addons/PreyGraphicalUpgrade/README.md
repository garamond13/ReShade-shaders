# PreyGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Optionally replace TAA (SMAA 2TX) with DLSS (DLAA).
- Replaced ambient occlusion (Screen Space Directional Occlusion->Full Resolution) with GTAO.
- Fixed and improved Screen Space Reflections.
- Adjustable vigenette strenghth.
- Optionally disable motion blur.
- Optionally disable lens effects.
- Fixed pixelated shadows.
- Force 16x anisotropic filtering.
- Upgraded render targets.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **PreyGraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **Prey.exe** is (ReShade should be installed in the same folder).

## Notes

 - "OPTIONS->VIDEO->ADVANCED SETTINGS->Adaptive Resolution" should be set to "OFF".
 - For DLSS, in "Configuration file(s) location/game.cfg" (see https://www.pcgamingwiki.com/wiki/Prey_(2017) ) `r_AntialiasingTAAPattern = 6` should be added. Make the game.cfg file read-only cause the game may overwrite it and revert the change.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add PreyGraphicalUpgrade project to Examples solution, then build the project.
