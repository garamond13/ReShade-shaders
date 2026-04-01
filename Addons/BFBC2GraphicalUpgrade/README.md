# BFBC2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- HBAO replaced with XeGTAO.
- Replaced bloom.
- Sharpening filter (Modified AMD FFX RCAS).
- Optionaly disable lens flare.
- 16x anisotropic filtering.
- Adjustable Tone Responce Curve (TRC) (sRGB, gamma).
- Greatly reduced input lag.
- Optionally force v-sync off.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **BFBC22GraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the games folder where **BFBC2Game.exe** is (ReShade should be installed in the same folder).

## Notes

- In game HBAO has to be enabled for XeGTAO to work.
- It works (should be used) with DirectX 11 only.
- If the game crashes on loading level use option (before loading the level) "Disable GraphicalUpgrade shaders" as a workaround.

## Compilation
- You can clone [ReShade](https://github.com/crosire/reshade) and add BFBC2GraphicalUpgrade project to Examples solution, then build the project.