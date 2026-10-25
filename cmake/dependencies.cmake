set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party dependencies statically" FORCE)

set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX2D_PROFILE OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)
set(BOX2D_COMPILE_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  box2d
  GIT_REPOSITORY https://github.com/erincatto/box2d.git
  GIT_TAG v3.2.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(box2d)

set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_PROFILE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  box3d
  GIT_REPOSITORY https://github.com/erincatto/box3d.git
  GIT_TAG v0.1.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(box3d)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG QUIET)
find_package(Threads REQUIRED)

if(TARGET glfw)
  set(BLB_GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
  set(BLB_GLFW_TARGET glfw3::glfw)
else()
  message(FATAL_ERROR "glfw3 was found, but it did not define a supported CMake target")
endif()

find_package(assimp 5.4 CONFIG QUIET)
if(NOT TARGET assimp::assimp AND NOT TARGET assimp)
  set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
  set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
  set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(assimp)
endif()

if(TARGET assimp::assimp)
  set(BLB_ASSIMP_TARGET assimp::assimp)
elseif(TARGET assimp)
  set(BLB_ASSIMP_TARGET assimp)
else()
  message(FATAL_ERROR "Assimp was not found or could not be built")
endif()

set(_blb_glslc_hints "")
if(DEFINED ENV{VULKAN_SDK})
  list(APPEND _blb_glslc_hints
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VULKAN_SDK}/Bin32"
    "$ENV{VULKAN_SDK}/bin"
  )
endif()

find_program(GLSLC NAMES glslc HINTS ${_blb_glslc_hints} REQUIRED)
unset(_blb_glslc_hints)
