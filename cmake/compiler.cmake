if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_blb_asan_default ON)
else()
  set(_blb_asan_default OFF)
endif()

option(BULBA_ENABLE_ASAN "Enable AddressSanitizer for Debug targets" ${_blb_asan_default})

function(blb_apply_compiler_settings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE
      "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall>"
      "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wextra>"
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>"
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>"
    )
  endif()

  target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:DEBUG>")

  if(BULBA_ENABLE_ASAN)
    if(MSVC)
      target_compile_options(${target} PRIVATE "$<$<CONFIG:Debug>:/fsanitize=address>")
      target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/fsanitize=address>")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(${target} PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address>"
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
      )
      target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:-fsanitize=address>")
    else()
      message(WARNING "AddressSanitizer is enabled but unsupported by this compiler configuration")
    endif()
  endif()
endfunction()
