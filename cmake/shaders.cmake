set(SHADER_SOURCE_DIR
  ${CMAKE_SOURCE_DIR}/assets/shaders
)

set(SHADER_BINARY_DIR
  ${CMAKE_BINARY_DIR}/shaders
)

set(SHADER_GENERATED_DIR
  ${CMAKE_BINARY_DIR}/generated/shaders
)

file(MAKE_DIRECTORY
  ${SHADER_BINARY_DIR}
  ${SHADER_GENERATED_DIR}
)

set(SHADER_NAMES
  basic3d.vert
  basic3d.frag
  basic2d.vert
  basic2d.frag
  text3d.vert
  text3d.frag
  text2d.vert
  text2d.frag
  shadow.vert
)

set(SHADER_SPV_OUTPUTS)
set(SHADER_GENERATED_SOURCES)

foreach(SHADER_NAME IN LISTS SHADER_NAMES)
  set(SHADER_INPUT
    ${SHADER_SOURCE_DIR}/${SHADER_NAME}
  )

  set(SHADER_SPV
    ${SHADER_BINARY_DIR}/${SHADER_NAME}.spv
  )

  string(REPLACE "." "_" SHADER_ID "${SHADER_NAME}")

  set(SHADER_HEADER
    ${SHADER_GENERATED_DIR}/${SHADER_ID}.h
  )

  set(SHADER_SOURCE
    ${SHADER_GENERATED_DIR}/${SHADER_ID}.c
  )

  add_custom_command(
    OUTPUT
    ${SHADER_SPV}

    COMMAND
    ${GLSLC}
    ${SHADER_INPUT}
    -o
    ${SHADER_SPV}

    DEPENDS
    ${SHADER_INPUT}

    VERBATIM
  )

  add_custom_command(
    OUTPUT
    ${SHADER_HEADER}
    ${SHADER_SOURCE}

    COMMAND
    ${CMAKE_COMMAND}
    -DINPUT=${SHADER_SPV}
    -DOUTPUT_H=${SHADER_HEADER}
    -DOUTPUT_C=${SHADER_SOURCE}
    -DNAME=${SHADER_ID}
    -P ${CMAKE_SOURCE_DIR}/cmake/embed_shader.cmake

    DEPENDS
    ${SHADER_SPV}
    ${CMAKE_SOURCE_DIR}/cmake/embed_shader.cmake

    VERBATIM
  )

  list(APPEND SHADER_SPV_OUTPUTS
    ${SHADER_SPV}
  )

  list(APPEND SHADER_GENERATED_SOURCES
    ${SHADER_SOURCE}
  )
endforeach()

add_custom_target(
  bulba_shaders
  DEPENDS
  ${SHADER_SPV_OUTPUTS}
  ${SHADER_GENERATED_SOURCES}
)

add_dependencies(
  bulba_engine
  bulba_shaders
)

target_include_directories(
  bulba_engine
  PRIVATE
  ${SHADER_GENERATED_DIR}
)

target_sources(
  bulba_engine
  PRIVATE
  ${SHADER_GENERATED_SOURCES}
)

target_compile_definitions(
  bulba_engine
  PRIVATE
  BULBA_USE_FREETYPE
)
