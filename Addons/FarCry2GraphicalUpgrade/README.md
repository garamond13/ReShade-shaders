# FarCry2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Optional ambient occlusion (Visibility Bitmask - GTAO).
- Optinal alternative tonemapping.
- Sharpening filter (Modified AMD FFX CAS).
- Fixed/improved bloom.
- Fixed the DX10 black square bug.
- Adjustable bloom intensity.
- 16x anisotropic filtering.
- Upgraded render targets (greatly reduces color banding).
- Improved some shaders.
- 10bit color output.
- Greatly reduced input lag if GPU bound.

## Usage

- Install ReShade 6.8.0+ with full add-on support.
- Copy **FarCry2GraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the games folder where **farcry2.exe** is (ReShade should be installed in the same folder).

## Notes

- It works (should be used) with DirectX 10 only.

## Known issues

- Save game image may apear black or wrong in other ways. This does not corupt the save file in any other way, the save file should still work perfectly fine.
- Wrong swapchain resolution or wrong window size. **SOLUTION:** Start the game with `-RenderProfile_Fullscreen 0 -borderless` command line argument, or change in game resolution, or in `%USERPROFILE%\Documents\My Games\Far Cry 2\GamerProfile.xml` make sure you have `Fullscreen="1"` set.

## Compilation

- You can clone [ReShade](https://github.com/crosire/reshade) and add FarCry2GraphicalUpgrade project to Examples solution, then build the project.