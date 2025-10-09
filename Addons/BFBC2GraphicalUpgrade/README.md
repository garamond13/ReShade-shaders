# BFBC2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- HBAO replaced with XeGTAO.
- Sharpening filter (Modified AMD FFX CAS).
- Optionaly disable lens flare.
- 16x anisotropic filtering.
- 10bit color output.
- Greatly reduced input lag if GPU bound.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **BFBC22GraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the games folder where **BFBC2Game.exe** is (ReShade should be installed in the same folder).
- Note that in game HBAO has to be enabled for XeGTAO to work.
- Note that it works (should be used) with DirectX 11 only.

## Compilation
- You can clone [ReShade](https://github.com/crosire/reshade) and add BFBC2GraphicalUpgrade project to Examples solution, then build the project.