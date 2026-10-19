# BioshockGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- XeGTAO or Visibility Bitmask - GTAO ambient occlusion.
- SMAA anti aliasing.
- Replaced bloom.
- 16x anisotropic filtering.
- Sharpening filter (Modified AMD FFX CAS).
- Temporal film grain (AMD FFX LFGA).
- Accurate FPS limiter.
- Greatly improved tonemapping.
- Upgraded render targets (greatly reduces color banding).
- 10bit color output.
- Greatly reduced input lag.
- Exit game! button, for quick exit. (The game has issue that if exiting the game via game's main menu, the game may stuck working in background.)

## Usage

- Install ReShade 6.8.0 or newer with full add-on support.
- Copy **BioshockGraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the game folder where **Bioshock.exe** is (ReShade should be installed in the same folder).

## Notes

- It works (should be used) with DirectX 10 only.
- In the game's settings "OPTIONS->GRAPHICS OPTIONS->Graphics Quality: High" is recomenended, or at minimum "OPTIONS->GRAPHICS OPTIONS->High Detail Post Processing" needs to be set to "ON".
- After starting the game and loading a save game GTAO may darken everything. This happens cause GTAO didn't get needed data from the game yet. Solution, try to move around, or try to temporarily load a different save game.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add BioshockGraphicalUpgrade project to Examples solution, then build the project.