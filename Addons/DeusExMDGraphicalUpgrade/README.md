# DeusExMDGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Optionally replace TAA with DLSS (DLAA).
- Replaced Sharpen with Modified AMD FFX CAS.
- Optionally disable last known location marker.
- Accurate FPS limiter.
- Upgraded render targets.
- 10bit color output.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.3 or newer with full add-on support.
- Copy **DeusExMDGraphicalUpgrade.addon64**, **nvngx_dlss.dll** and **GraphicalUpgrade** folder into the game folder where **DXMD.exe** is (ReShade should be installed in the same folder).

## Notes

- It works (should be used) with DirectX 11 only.
- If "OPTIONS->DISPALY->Exclusive fullscreen" is off "OPTIONS->DISPALY->Resolution" should be set to highest available.
- "OPTIONS->DISPLAY->Fullscreen exclusive" should be turned off in game's settings otherwise DLSS may fail to load, or the game may crash on launch. If this doesn't fix the issue, launch the game with DLSS disabled in the DeusExMDGraphicalUpgrade and than in the game's settings toggle (on/off) "OPTIONS->DISPLAY->Fullscreen". After this DLSS should work properly if enabled. This entire issue may be exclusive to Epic Games version of the game.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add DeusExMDGraphicalUpgrade project to Examples solution, then build the project.