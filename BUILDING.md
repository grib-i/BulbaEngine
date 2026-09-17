# Building BulbaEngine

BulbaEngine keeps platform-specific code behind `core/platform.*` and keeps the renderer inside `core/render` with Vulkan as the current graphics implementation.

## Linux

```sh
cmake -S . -B build/linux-debug -DCMAKE_BUILD_TYPE=Debug -DPROJECT_NAME=bulba
cmake --build build/linux-debug -j
./build/linux-debug/bulba
```

## Windows

```bat
build.bat Debug
```

The Windows build expects a working CMake toolchain, GLFW, Vulkan SDK and FreeType installation available to CMake. The engine does not use POSIX-only timing or dynamic-library APIs directly.

## Android

The engine core is prepared for an Android platform layer through `BLB_PLATFORM_ANDROID` and `core/platform.*`. The desktop GLFW window layer is intentionally not presented as the Android application shell; Android surface, lifecycle and input integration should be supplied by the Android frontend while reusing the same engine library and renderer API.
