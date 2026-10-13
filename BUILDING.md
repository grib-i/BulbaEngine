# Building BulbaEngine

BulbaEngine keeps platform-specific code behind `core/platform.*` and keeps the renderer inside `core/render` with Vulkan as the current graphics implementation.

## Linux

```sh
cmake -S . -B build/linux-debug -DCMAKE_BUILD_TYPE=Debug -DPROJECT_NAME=bulba
cmake --build build/linux-debug -j
./build/linux-debug/bulba
```

## Windows (MSVC x64)

Prerequisites:

- Visual Studio 2022 with **Desktop development with C++** (MSVC x64 and Windows SDK).
- CMake and the Vulkan SDK; `VULKAN_SDK` must be set and `%VULKAN_SDK%\Bin\glslc.exe` must exist.
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set to its root folder (for example, `C:\vcpkg`). The project manifest installs GLFW, FreeType, PNG, JPEG and Assimp dependencies automatically during CMake configuration.

Run from the repository root in a terminal where `VULKAN_SDK` and `VCPKG_ROOT` are available:

```bat
install.bat debug
```

This configures with the Visual Studio 2022 generator, builds the Debug sample application, and starts it. To build and install the Release DLL to `C:\Bulba`, run:

```bat
install.bat release
```

GitHub Actions uses the same MSVC x64 generator and `x64-windows-static` vcpkg triplet. Windows uses Win32 thread and timing APIs; the POSIX implementations remain enabled on Linux.

## Android

The engine core is prepared for an Android platform layer through `BLB_PLATFORM_ANDROID` and `core/platform.*`. The desktop GLFW window layer is intentionally not presented as the Android application shell; Android surface, lifecycle and input integration should be supplied by the Android frontend while reusing the same engine library and renderer API.
