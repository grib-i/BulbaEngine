if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(
    bulba_engine
    PRIVATE
    DEBUG
  )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    bulba_engine
    PRIVATE
    -fsanitize=address
    -fno-omit-frame-pointer
  )

  target_link_options(
    bulba_engine
    INTERFACE
    -fsanitize=address
  )
endif()
