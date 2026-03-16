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

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add DeusExMDGraphicalUpgrade project to Examples solution, then build the project.