cmake_minimum_required(VERSION 3.18)

make_directory("${CMAKE_BINARY_DIR}/shaders/")
make_directory("${CMAKE_BINARY_DIR}/shader_headers/")

function(MAKE_SHADER_HEADER shader_path)

if (EXISTS "${shader_path}")

get_filename_component(SHADERNAME "${shader_path}" NAME)
add_custom_command(
  OUTPUT
    "${CMAKE_BINARY_DIR}/shaders/${SHADERNAME}.spv"
  COMMAND
    "${Vulkan_GLSLC_EXECUTABLE}" "-O" "-fentry-point=main" "--target-env=vulkan1.1"
      "${shader_path}" -o "${CMAKE_BINARY_DIR}/shaders/${SHADERNAME}.spv"
  DEPENDS
    "${shader_path}"
  IMPLICIT_DEPENDS
    "${shader_path}"
  COMMENT
    "Compiling shader ${SHADERNAME}"
)

set(C_SAFE_SHADERNAME "${SHADERNAME}")
string(REPLACE "." "_" C_SAFE_SHADERNAME ${C_SAFE_SHADERNAME})

add_custom_command(
  OUTPUT
    "${CMAKE_BINARY_DIR}/shader_headers/${SHADERNAME}.spv.h"
  COMMAND ${CMAKE_COMMAND}
    "-DSHADER_HEADER_OUTPUT=${CMAKE_BINARY_DIR}/shader_headers/${SHADERNAME}.spv.h"
    "-DSHADERNAME=${C_SAFE_SHADERNAME}"
    "-DSHADER_SPIRV_INPUT=${CMAKE_BINARY_DIR}/shaders/${SHADERNAME}.spv"
    "-P"
    "${CMAKE_SOURCE_DIR}/cmake/HeaderFromSPIRV.cmake"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/"
  DEPENDS
    "${CMAKE_BINARY_DIR}/shaders/${SHADERNAME}.spv"
  IMPLICIT_DEPENDS
    "${CMAKE_BINARY_DIR}/shaders/${SHADERNAME}.spv"
  COMMENT "Building shader header '${SHADERNAME}.spv.h'"
  VERBATIM
)

endif (EXISTS "${shader_path}")

endfunction(MAKE_SHADER_HEADER shader_path)
