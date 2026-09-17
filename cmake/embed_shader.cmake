file(READ "${INPUT}" SHADER_HEX HEX)

string(LENGTH "${SHADER_HEX}" HEX_LENGTH)

math(EXPR SHADER_SIZE
  "${HEX_LENGTH} / 2"
)

set(BYTE_VALUES "")

math(EXPR LAST_INDEX
  "${HEX_LENGTH} - 2"
)

foreach(INDEX RANGE 0 ${LAST_INDEX} 2)
  string(SUBSTRING "${SHADER_HEX}" ${INDEX} 2 BYTE)

  if(BYTE_VALUES STREQUAL "")
    set(BYTE_VALUES "0x${BYTE}")
  else()
    string(APPEND BYTE_VALUES ", 0x${BYTE}")
  endif()
endforeach()

file(WRITE "${OUTPUT_H}"
"#ifndef BLB_SHADER_${NAME}_H
#define BLB_SHADER_${NAME}_H

#include <stddef.h>

extern const unsigned char blb_shader_${NAME}[];
extern const size_t blb_shader_${NAME}_size;

#endif
")

file(WRITE "${OUTPUT_C}"
"#include \"${NAME}.h\"

const unsigned char blb_shader_${NAME}[] = {
${BYTE_VALUES}
};

const size_t blb_shader_${NAME}_size = ${SHADER_SIZE};
")
