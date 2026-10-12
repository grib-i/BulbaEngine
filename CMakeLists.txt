cmake_minimum_required(VERSION 3.22)

# Match the x64-windows-static vcpkg triplet: use the static MSVC runtime.
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT PROJECT_NAME)
  set(PROJECT_NAME "BulbaEngine")
endif()

project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static dependency libraries need PIC when linked into libbulba_engine.so.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# Box2D
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
  GIT_TAG v3.1.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(box2d)

# Box3D
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

find_program(GLSLC glslc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
  REQUIRED
)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/bulba/*.c"
)

option(BLB_BUILD_SHARED "Build bulba_engine as a shared library" ON)
if(BLB_BUILD_SHARED)
  add_library(bulba_engine SHARED ${ENGINE_SOURCES})
else()
  add_library(bulba_engine STATIC ${ENGINE_SOURCES})
endif()

set_target_properties(bulba_engine PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "0"
  POSITION_INDEPENDENT_CODE ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(bulba_engine
  PUBLIC "${CMAKE_SOURCE_DIR}/include"
)

include("${CMAKE_SOURCE_DIR}/cmake/platform.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/compiler.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/shaders.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/textures.cmake")

# On Windows the vcpkg x64-windows-static triplet links these dependencies
# statically where supported. Vulkan's loader remains a runtime dependency.
target_link_libraries(bulba_engine
  PUBLIC
    Vulkan::Vulkan
    glfw
    Freetype::Freetype
    PNG::PNG
    ${BLB_ASSIMP_TARGET}
    box2d::box2d
    box3d::box3d
    Threads::Threads
)

if(UNIX AND NOT APPLE)
  target_link_libraries(bulba_engine PUBLIC m dl)
endif()

if(JPEG_FOUND)
  target_link_libraries(bulba_engine PRIVATE JPEG::JPEG)
  target_compile_definitions(bulba_engine PRIVATE BLB_HAVE_JPEG=1)
endif()

include("${CMAKE_SOURCE_DIR}/cmake/options.cmake")
target_compile_definitions(bulba_engine PUBLIC
  VULKAN_SHADOW_MAP_SIZE=${BLB_SHADOW_MAP_SIZE}
  BLB_GLOW_STEPS_3D=${BLB_GLOW_STEPS_3D}
  BLB_GLOW_STEPS_2D=${BLB_GLOW_STEPS_2D}
)

target_compile_definitions(bulba_engine PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# The checked-in src/main.c calls BLB_TestHandlers(), so test sources must be
# linked in Release too. This target currently acts as a test/sample app.
if(BLB_BUILD_APP)
  set(APP_SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")
  file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/*.c"
  )
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/object_count_3d\\.c$")
  list(APPEND APP_SOURCES ${TEST_SOURCES})

  add_executable(${PROJECT_NAME} ${APP_SOURCES})
  target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/tests")
  target_link_libraries(${PROJECT_NAME} PRIVATE bulba_engine)
  target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
endif()

include(GNUInstallDirs)
if(UNIX AND NOT APPLE AND CMAKE_LIBRARY_ARCHITECTURE)
  # Debian multiarch library directory so the dynamic loader can find the .so.
  set(CMAKE_INSTALL_LIBDIR "lib/${CMAKE_LIBRARY_ARCHITECTURE}" CACHE PATH "Library install directory" FORCE)
endif()

if(UNIX AND NOT APPLE AND BLB_BUILD_APP)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
  )
endif()
if(UNIX AND NOT APPLE AND BLB_BUILD_SHARED)
  set_target_properties(bulba_engine PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Windows: place the application DLLs beside the EXE and collect runtime DLLs
# required by the executable/library. Do not copy Windows system DLLs.
if(WIN32)
  install(TARGETS bulba_engine
    RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  if(BLB_BUILD_APP)
    install(TARGETS ${PROJECT_NAME}
      RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()
  install(RUNTIME_DEPENDENCY_SET BulbaRuntimeDependencies
    DIRECTORIES "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/Bin32"
    PRE_EXCLUDE_REGEXES "api-ms-win-.*" "ext-ms-win-.*"
    POST_EXCLUDE_REGEXES ".*[Ww]indows[/\\\\][Ss]ystem32[/\\\\].*" ".*[Ww]indows[/\\\\][Ss]ys[Ww][Oo][Ww]64[/\\\\].*"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
else()
  install(TARGETS bulba_engine
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  if(BLB_BUILD_APP)
    install(TARGETS ${PROJECT_NAME}
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# CPack creates a Debian package on Linux and an NSIS installer on Windows.
set(CPACK_PACKAGE_NAME "bulba-engine")
set(CPACK_PACKAGE_VENDOR "BulbaEngine")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BulbaEngine and its shared library")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BulbaEngine")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

if(WIN32)
  set(CPACK_GENERATOR "NSIS")
  set(CPACK_PACKAGE_FILE_NAME "BulbaEngine-${PROJECT_VERSION}-Windows-x64")
  set(CPACK_NSIS_DISPLAY_NAME "BulbaEngine ${PROJECT_VERSION}")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH OFF)
elseif(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_FILE_NAME "bulba-engine-${PROJECT_VERSION}-Linux-x86_64")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "BulbaEngine maintainers")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/packaging/linux/postinst;${CMAKE_SOURCE_DIR}/packaging/linux/postrm"
  )
endif()

include(CPack)
