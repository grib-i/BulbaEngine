include(CTest)

if(NOT BLB_BUILD_APP AND NOT BUILD_TESTING)
  return()
endif()

file(GLOB BLB_CHECK_SOURCES CONFIGURE_DEPENDS "${BLB_SOURCE_DIR}/tests/checks/*.c")
file(GLOB BLB_DEMO_SOURCES CONFIGURE_DEPENDS "${BLB_SOURCE_DIR}/tests/demos/*.c")

add_library(bulba_test_support STATIC ${BLB_CHECK_SOURCES} ${BLB_DEMO_SOURCES})
target_include_directories(bulba_test_support PUBLIC "${BLB_SOURCE_DIR}/tests" "${BLB_SOURCE_DIR}/src")
target_link_libraries(bulba_test_support PUBLIC bulba_engine)
if(UNIX AND NOT APPLE)
  target_link_libraries(bulba_test_support PUBLIC m)
endif()
file(MAKE_DIRECTORY "${BLB_BINARY_DIR}/test-output")
target_compile_definitions(bulba_test_support PRIVATE
  BLB_TEST_OUTPUT_DIR="${BLB_BINARY_DIR}/test-output"
  BLB_TEST_SHADER_DIR="${SHADER_BINARY_DIR}"
)
blb_apply_compiler_settings(bulba_test_support)

if(BLB_BUILD_APP)
  target_link_libraries(${BLB_APP_NAME} PRIVATE bulba_test_support)
endif()

if(BUILD_TESTING)
  add_executable(bulba_tests "${BLB_SOURCE_DIR}/src/main.c")
  target_link_libraries(bulba_tests PRIVATE bulba_test_support)
  blb_apply_compiler_settings(bulba_tests)

  foreach(suite IN ITEMS io shaders audio core resources physics2d physics3d particles2d particles3d handlers assets spatial renderer orbits)
    add_test(NAME unit.${suite} COMMAND bulba_tests unit ${suite})
    set_tests_properties(unit.${suite} PROPERTIES TIMEOUT 120 LABELS "unit" WORKING_DIRECTORY "${BLB_SOURCE_DIR}")
  endforeach()

  add_test(NAME memory.lifecycle COMMAND bulba_tests memory)
  set_tests_properties(memory.lifecycle PROPERTIES TIMEOUT 180 LABELS "memory;stress" WORKING_DIRECTORY "${BLB_SOURCE_DIR}")

  # Parser regressions must fail before opening a window or starting long runs.
  add_test(NAME cli.reject_zero_count COMMAND bulba_tests demo object_count_3d --count 0)
  add_test(NAME cli.reject_nan_speed COMMAND bulba_tests demo primitives_3d --speed nan)
  add_test(NAME cli.reject_zero_samples COMMAND bulba_tests bench --samples 0)
  add_test(NAME cli.reject_zero_ticks COMMAND bulba_tests memory --ticks 0)
  add_test(NAME cli.reject_zero_repeat COMMAND bulba_tests unit core --repeat 0)
  add_test(NAME cli.reject_unknown_option COMMAND bulba_tests demo skybox --unknown 1)
  add_test(NAME cli.reject_model_size COMMAND bulba_tests demo model_fbx --model-size nan)
  add_test(NAME cli.reject_orbit_step COMMAND bulba_tests demo solar_system --orbit-step 0)
  add_test(NAME cli.reject_layout_samples COMMAND bulba_tests demo model_gltf --layout-samples 0)
  set_tests_properties(cli.reject_zero_count cli.reject_nan_speed cli.reject_zero_samples
    cli.reject_zero_ticks cli.reject_zero_repeat cli.reject_unknown_option cli.reject_model_size cli.reject_layout_samples cli.reject_orbit_step
    PROPERTIES WILL_FAIL TRUE TIMEOUT 10 LABELS "cli" WORKING_DIRECTORY "${BLB_SOURCE_DIR}")

  add_test(NAME cli.configured_unit COMMAND bulba_tests unit core --repeat 2)
  add_test(NAME cli.configured_memory COMMAND bulba_tests memory --cycles 17 --ticks 3)
  add_test(NAME cli.configured_bench COMMAND bulba_tests bench --samples 3 --iterations 10)
  set_tests_properties(cli.configured_unit cli.configured_memory cli.configured_bench
    PROPERTIES TIMEOUT 60 LABELS "cli" WORKING_DIRECTORY "${BLB_SOURCE_DIR}")

  # Opt in only on a machine with a working Vulkan device and display server.
  option(BLB_TEST_VISUAL "Register short windowed demo smoke tests" OFF)
  if(BLB_TEST_VISUAL)
    foreach(demo IN ITEMS
        primitives_2d textures_2d sprite_animation text_2d particles_2d particles_3d
        primitives_3d textures_3d materials_3d lights_3d physics_2d physics_3d
        model_obj model_glb model_fbx model_gltf skybox object_count_2d object_count_3d
        transparency transparency_3d solar_system lod input lights_2d)
      add_test(NAME demo.${demo} COMMAND bulba_tests demo ${demo} --frames 60)
      set_tests_properties(demo.${demo} PROPERTIES TIMEOUT 120 LABELS "visual" RUN_SERIAL TRUE WORKING_DIRECTORY "${BLB_SOURCE_DIR}")
    endforeach()
  endif()
endif()
