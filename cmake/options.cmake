option(BLB_BUILD_APP "Build the desktop sample application" OFF)
option(BLB_BUILD_SHARED "Build bulba_engine as a shared library" ON)

set(BLB_SHADOW_MAP_SIZE 1024 CACHE STRING "Shadow map resolution per face")
set(BLB_GLOW_STEPS_3D 8 CACHE STRING "3D glow shell passes")
set(BLB_GLOW_STEPS_2D 8 CACHE STRING "2D glow shell passes")

set(BLB_APP_NAME "BulbaEngine" CACHE STRING "Demo entry-point executable name")
