# KingdomComeDeliveranceGraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Optionally replace TAA (SMAA 2TX) with DLSS (DLAA only).
- Upgraded render targets.

## Usage

- Install ReShade 6.8.0 or newer with full add-on support.
- Copy **KingdomComeDeliveranceGraphicalUpgrade.addon64** and **GraphicalUpgrade** folder in the game folder where **KingdomCome.exe.exe** is (ReShade should be installed in the same folder).
- For DLSS `r_AntialiasingTAAPattern 4` should be set. You can look at [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Kingdom_Come:_Deliverance) for ways to set it.

## Notes

- Motion blur looks bad if used with DLSS.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add KingdomComeDeliveranceGraphicalUpgrade project to Examples solution, then build the project.