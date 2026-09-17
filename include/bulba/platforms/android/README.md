# Android platform boundary

The engine core is kept independent from the Android application lifecycle. An Android frontend should provide:

- Activity or NativeActivity lifecycle handling
- Vulkan surface creation from the Android window
- touch, sensor and gamepad input
- application asset access
- pause/resume and surface recreation

The shared engine layer remains responsible for scene, entities, components, physics, materials, shaders, scripting and plugin-facing APIs.
