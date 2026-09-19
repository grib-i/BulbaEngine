set(BLB_TEXTURE_GENERATED_DIR
    ${CMAKE_BINARY_DIR}/generated/textures
)

file(MAKE_DIRECTORY ${BLB_TEXTURE_GENERATED_DIR})

add_executable(
  blb_png_to_c
  ${CMAKE_SOURCE_DIR}/cmake/png_to_c.c
)

target_link_libraries(
  blb_png_to_c
  PRIVATE
  PNG::PNG
)

file(GLOB BLB_TEXTURE_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/assets/textures/*.png
  ${CMAKE_SOURCE_DIR}/assets/textures/*.PNG
)

target_include_directories(
  bulba_engine
  PRIVATE
  ${BLB_TEXTURE_GENERATED_DIR}
)

foreach(TEXTURE_FILE IN LISTS BLB_TEXTURE_FILES)

  get_filename_component(
    TEXTURE_NAME
    ${TEXTURE_FILE}
    NAME_WE
  )

  string(
    MAKE_C_IDENTIFIER
    "${TEXTURE_NAME}"
    TEXTURE_ID
  )

  set(OUTPUT_C
    ${BLB_TEXTURE_GENERATED_DIR}/${TEXTURE_NAME}.c
  )

  set(OUTPUT_H
    ${BLB_TEXTURE_GENERATED_DIR}/${TEXTURE_NAME}.h
  )

  add_custom_command(
    OUTPUT
      ${OUTPUT_C}
      ${OUTPUT_H}

    COMMAND
      $<TARGET_FILE:blb_png_to_c>
      ${TEXTURE_FILE}
      ${OUTPUT_C}
      ${OUTPUT_H}
      BLB_TEXTURE_${TEXTURE_ID}

    DEPENDS
      ${TEXTURE_FILE}
      blb_png_to_c

    VERBATIM
  )

  target_sources(
    bulba_engine
    PRIVATE
      ${OUTPUT_C}
      ${OUTPUT_H}
  )

endforeach()
