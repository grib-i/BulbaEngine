option(BLB_BUILD_APP "Build the desktop sample application" ON)
option(BLB_BUILD_SHADERS "Build built-in SPIR-V shaders" ON)


# Runtime-friendly quality knobs. Lower values help integrated/low-end GPUs.
set(BLB_SHADOW_MAP_SIZE 1024 CACHE STRING "Shadow map resolution per face")
set(BLB_GLOW_STEPS_3D 8 CACHE STRING "3D glow shell passes")
set(BLB_GLOW_STEPS_2D 8 CACHE STRING "2D glow shell passes")
