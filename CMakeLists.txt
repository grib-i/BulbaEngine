cmake_minimum_required(VERSION 3.20)

if(NOT PROJECT_NAME)
  set(PROJECT_NAME "DEFAULT")
endif()

project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)

find_program(GLSLC glslc REQUIRED)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/bulba/*.c
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_library(
    bulba_engine
    SHARED
    ${ENGINE_SOURCES}
  )
else()
  add_library(
    bulba_engine
    STATIC
    ${ENGINE_SOURCES}
  )
endif()

target_include_directories(
  bulba_engine
  PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/platform.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/compiler.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/shaders.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/textures.cmake)

target_link_libraries(
  bulba_engine
  PUBLIC
  Vulkan::Vulkan
  glfw
  Freetype::Freetype
  PNG::PNG
  m
  dl
)

if(UNIX AND NOT APPLE)
  target_link_libraries(
    bulba_engine
    PUBLIC
    m
    dl
  )
endif()

include(${CMAKE_SOURCE_DIR}/cmake/options.cmake)

target_compile_definitions(
  bulba_engine
  PUBLIC
  VULKAN_SHADOW_MAP_SIZE=${BLB_SHADOW_MAP_SIZE}
  BLB_GLOW_STEPS_3D=${BLB_GLOW_STEPS_3D}
  BLB_GLOW_STEPS_2D=${BLB_GLOW_STEPS_2D}
)

if(BLB_BUILD_APP)
  set(APP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.c
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
      ${CMAKE_SOURCE_DIR}/tests/*.c
    )

    list(APPEND APP_SOURCES
      ${TEST_SOURCES}
    )
  endif()

  add_executable(
    ${PROJECT_NAME}
    ${APP_SOURCES}
  )

  target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
  )

  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    bulba_engine
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(
      ${PROJECT_NAME}
      PRIVATE
      DEBUG
    )
  endif()
endif()

install(
  TARGETS bulba_engine
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION include
)
