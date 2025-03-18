cmake_minimum_required(VERSION 3.18)

file (WRITE "${SHADER_HEADER_OUTPUT}"
  "#pragma once\n\n#include <stdint.h>\n\n")
file (APPEND "${SHADER_HEADER_OUTPUT}"
  "static const uint32_t shader__${SHADERNAME}[] = {\n")

file(SIZE "${SHADER_SPIRV_INPUT}" THIS_FILE_SIZE)

foreach(i RANGE 0 ${THIS_FILE_SIZE} 512 )

  file(READ "${SHADER_SPIRV_INPUT}" BINARY_LINE OFFSET ${i} LIMIT 512 HEX )

  set(OUTPUT_LINE "")

  string(LENGTH "${BINARY_LINE}" BINARY_LINE_LENGTH)

  if (BINARY_LINE_LENGTH GREATER 0)
    math(EXPR BINARY_LINE_LASTCHAR "${BINARY_LINE_LENGTH} - 1")
    string(APPEND OUTPUT_LINE "  ")
    foreach(j RANGE 0 ${BINARY_LINE_LASTCHAR} 8)
      string(APPEND OUTPUT_LINE "0x")
      foreach(i RANGE 6 0 -2)
        math(EXPR jPLUS "${j} + ${i}")
        string(SUBSTRING "${BINARY_LINE}" ${jPLUS} 2 THIS_BYTE)
        string(APPEND OUTPUT_LINE "${THIS_BYTE}")
      endforeach(i RANGE 6 0 -2)
      string(APPEND OUTPUT_LINE ", ")
    endforeach(j RANGE 0 ${BINARY_LINE_LASTCHAR} 8)
    string(APPEND OUTPUT_LINE "\n")
    file(APPEND "${SHADER_HEADER_OUTPUT}" "${OUTPUT_LINE}")
  endif (BINARY_LINE_LENGTH GREATER 0)

endforeach(i RANGE 0 ${THIS_FILE_SIZE} 512 )

file (APPEND "${SHADER_HEADER_OUTPUT}"
  "};\n\n")
