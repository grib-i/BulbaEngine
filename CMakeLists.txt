cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(BLB_VERSION "1.0.2" CACHE STRING "BulbaEngine version")
project(BulbaEngine VERSION "${BLB_VERSION}" LANGUAGES C CXX)

option(BUILD_TESTING "Build BulbaEngine tests" OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

set(BLB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BLB_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(BLB_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/BulbaEngine")

include("${BLB_SOURCE_DIR}/cmake/options.cmake")
include("${BLB_SOURCE_DIR}/cmake/compiler.cmake")

include("${BLB_SOURCE_DIR}/cmake/dependencies.cmake")

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${BLB_SOURCE_DIR}/src/bulba/*.c"
)

if(BLB_BUILD_SHARED)
  add_library(bulba_engine SHARED ${ENGINE_SOURCES})
else()
  add_library(bulba_engine STATIC ${ENGINE_SOURCES})
endif()

set_target_properties(bulba_engine PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "0"
  EXPORT_NAME BulbaEngine
  POSITION_INDEPENDENT_CODE ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(BulbaEngine::BulbaEngine ALIAS bulba_engine)

target_include_directories(bulba_engine
  PUBLIC
    $<BUILD_INTERFACE:${BLB_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    "${BLB_SOURCE_DIR}/src"
)

target_link_libraries(bulba_engine
  PUBLIC
    Vulkan::Vulkan
    ${BLB_GLFW_TARGET}
  PRIVATE
    Freetype::Freetype
    PNG::PNG
    ${BLB_ASSIMP_TARGET}
    box2d::box2d
    box3d::box3d
    Threads::Threads
)

if(UNIX AND NOT APPLE)
  target_link_libraries(bulba_engine PRIVATE m dl)
endif()

if(JPEG_FOUND)
  target_link_libraries(bulba_engine PRIVATE JPEG::JPEG)
  target_compile_definitions(bulba_engine PRIVATE BLB_HAVE_JPEG=1)
endif()

include("${BLB_SOURCE_DIR}/cmake/platform.cmake")
include("${BLB_SOURCE_DIR}/cmake/shaders.cmake")
include("${BLB_SOURCE_DIR}/cmake/textures.cmake")

blb_apply_compiler_settings(bulba_engine)

target_compile_definitions(bulba_engine PUBLIC
  VULKAN_SHADOW_MAP_SIZE=${BLB_SHADOW_MAP_SIZE}
  BLB_GLOW_STEPS_3D=${BLB_GLOW_STEPS_3D}
  BLB_GLOW_STEPS_2D=${BLB_GLOW_STEPS_2D}
)

include("${BLB_SOURCE_DIR}/cmake/app.cmake")
include("${BLB_SOURCE_DIR}/cmake/install.cmake")

if(BUILD_TESTING)
  include("${BLB_SOURCE_DIR}/cmake/tests.cmake")
endif()

# Optional Glowbound 2D platformer.
option(BLB_BUILD_GLOWBOUND "Build the Glowbound 2D platformer" OFF)
if(BLB_BUILD_GLOWBOUND)
  add_subdirectory(games/glowbound)
endif()
