## Usage

You can double click CMSLUTGenerator.exe to generate CMSLUT.dds file in the same folder using the default options.  
Or you can use it via command line (cmd.exe). Use `CMSLUTGenerator.exe -h` to print help and show the default options.

## Compilation

### Dependencies

- Little CMS
- DirectXTex

### Building from source on Windows
You can use Visual Studio 2022

To install dependencies you can use [vcpkg](https://github.com/microsoft/vcpkg).

You can install Little CMS with:  
`vcpkg install lcms:x64-windows-static --clean-after-build`

You can install DirectXTex with:  
`vcpkg install directxtex:x64-windows-static --clean-after-build`
