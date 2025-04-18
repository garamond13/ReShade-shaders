# Compilation

## Dependencies

- Little CMS
- DirectXTex

## Building from source on Windows
You can use Visual Studio 2022

To install dependencies you can use [vcpkg](https://github.com/microsoft/vcpkg).

You can install Little CMS with:
vcpkg install lcms:x64-windows-static --clean-after-build

You can install DirectXTex with:
vcpkg install directxtex:x64-windows-static --clean-after-build
