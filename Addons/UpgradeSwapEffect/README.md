## Usage

Install like any other addon.

On the first run it will create a config option in the ReShade.ini if it's not already there.  
`[APP]`  
`Force10BitFormat=0`

## Compilation

You can use Visual Studio 2022.

You can clone [ReShade](https://github.com/crosire/reshade) and add UpgradeSwapEffect project to Examples solution, then build the project.

Depends on [MinHook](https://github.com/TsudaKageyu/minhook)  
You can install it via vcpkg with `vcpkg install minhook:x64-windows-static --clean-after-build` and `vcpkg install minhook:x86-windows-static --clean-after-build`
