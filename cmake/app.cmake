if(NOT BLB_BUILD_APP)
  return()
endif()

add_executable(${BLB_APP_NAME} "${BLB_SOURCE_DIR}/src/main.c")
target_link_libraries(${BLB_APP_NAME} PRIVATE bulba_engine)
blb_apply_compiler_settings(${BLB_APP_NAME})
