# FarCry2GraphicalUpgrade

- Modern borderless window (flip mod, tearing).
- Sharpening filter (Modified AMD FFX CAS).
- 3D LUT aplicator.
- 16x anisotropic filtering.
- Upgraded render targets (greatly reduces color banding).
- 10bit color output.
- Fix for the DX10 black square bug.
- Greatly reduced input lag if GPU bound.

## Usage

- Install ReShade 6.6.0+ with full add-on support.
- Copy **FarCry2GraphicalUpgrade.addon32** and **GraphicalUpgrade** folder in the games folder where **farcry2.exe** is (ReShade should be installed in the same folder).
- If you wanna apply LUT, only 3D CUBE LUTs are supported. Put your **LUT.CUBE** (has to be named **LUT.CUBE**) file inside the **GraphicalUpgrade** folder, LUT will be auto detected and applied.

## Compilation
- You can clone [ReShade](https://github.com/crosire/reshade) and add FarCry2GraphicalUpgrade project to Examples solution, then build the project.