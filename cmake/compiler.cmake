option(BULBA_ENABLE_ASAN "Enable AddressSanitizer" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(
    bulba_engine
    PRIVATE
    DEBUG
  )
endif()

if(
  CMAKE_BUILD_TYPE STREQUAL "Debug"
  AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
  AND BULBA_ENABLE_ASAN
)
  add_compile_options(
    -g
    -O0
    -fno-omit-frame-pointer
  )

  add_link_options(
  )
endif()
