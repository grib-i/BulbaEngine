# BulbaEngine Architecture

BulbaEngine keeps engine-facing APIs under `include/bulba/core` and routes rendering through `core/render`.

`core/render` owns material and shader concepts. Vulkan remains the current graphics implementation under `graphics/vulkan` and is treated as a low-level rendering API layer.

Materials are shared reference-counted resources and work for both 2D and 3D. A material owns base color, emission, glow, render mode and future shader bindings. The current glow is a material-driven additive halo made from layered render passes; the material interface is ready for a future HDR bloom path.

Scenes assign stable entity IDs and carry a physics world. Objects expose component masks and 3D objects reserve rigid-body and collider references without coupling rendering to physics internals.

The physics API is intentionally separated into world, body, collider and shape types. The current implementation provides fixed-step integration and is prepared for broadphase, narrowphase, contacts, constraints and solving later.

Shaders are represented by shader assets rather than being hard-wired concepts. Built-in shaders remain the defaults, while custom shader source paths can be attached to a material and later compiled by the active graphics backend.

Plugins use a versioned C ABI. `.bpl` is reserved for a versioned package/container format while the loader currently operates on the contained native library. The plugin API exposes only stable callbacks instead of renderer internals.

Retro is separated behind a compiler boundary. The current tokenizer and C emission entry point are scaffolding for a future pipeline of lexer, parser, AST, IR and C backend.

## Platform boundary

Platform-sensitive facilities are routed through `core/platform.*`: monotonic time, string duplication and dynamic library loading. The current desktop frontend uses GLFW and Vulkan internally, while the engine-facing foundation avoids POSIX-only calls.

Linux and Windows are first-class desktop targets. Android is a planned mobile frontend: the same scene, component, physics, material, shader and plugin foundations can be reused while Android supplies lifecycle, window/surface and input integration.
