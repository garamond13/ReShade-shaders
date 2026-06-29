# MonsterHunterWorldGraphicalUpgrade

- Optionally replace TAA with DLSS (DLAA only).
- Replaced bloom.
- Improved shadows and ambient occlusion.
- Adjustable bloom intensity.
- Adjustable Tone Response Curve (TRC) (sRGB, gamma).
- Upgraded render targets.
- Greatly reduced input lag.

## Usage

- Install ReShade 6.7.1 or newer with full add-on support.
- Copy **MonsterHunterWorldGraphicalUpgrade.addon64**, **nvngx_dlss.dll** (replace the existing nvngx_dlss.dll) and **GraphicalUpgrade** folder into the game folder where **MonsterHunterWorld.exe** is (ReShade should be installed in the same folder).

## Notes

- Works, should be used only with DirectX 11.
- For ReShade to work will need to rename ReShade's "dxgi.dll" to "d3d11.dll".
- The game's TAA doesn't enable jitters! For TAA to make any sense (including DLSS) jitters need to be enabled. You can use this [mod](https://www.nexusmods.com/monsterhunterworld/mods/8418?tab=description) to enable them.
- "Options->Display->FidelityFX CAS + Upscaling" should be set to "Off".

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add MonsterHunterWorldGraphicalUpgrade project to Examples solution, then build the project.
