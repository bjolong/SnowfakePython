CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

PROJECT(SnowfakePython)

SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

SET( CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH}"
  "${CMAKE_SOURCE_DIR}/cmake"
)

include(FetchContent)

SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib /usr/local/lib)
find_package(
  Vulkan REQUIRED)
IF (NOT Vulkan_glslc_FOUND)
  message(ERROR "Could not find glslc compiler (install shaderc, spirv-tools?)")
ENDIF (NOT Vulkan_glslc_FOUND)

set(GLSL_SHADERS_TO_COMPILE
  "solver_substep.comp"
  "sample_occupancy.comp"
  "slines.vert"
  "slines.frag"
  "svolume.vert"
  "svolume.frag"
)

include (ShaderHeader)
foreach(SHADERNAME IN LISTS GLSL_SHADERS_TO_COMPILE)
  MAKE_SHADER_HEADER("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${SHADERNAME}")
endforeach(SHADERNAME IN LISTS GLSL_SHADERS_TO_COMPILE)

add_custom_target(compile_shaders_to_headers
  DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/solver_substep.comp.spv.h"
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/sample_occupancy.comp.spv.h"
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/svolume.vert.spv.h"
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/svolume.frag.spv.h"
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/slines.vert.spv.h"
    "${CMAKE_CURRENT_BINARY_DIR}/shader_headers/slines.frag.spv.h"
)

option(ENABLE_TESTS "Build shader testing" OFF)
option(ENABLE_GUI "Build with GUI" ON)
option(ENABLE_CPP_EXECUTABLE "Build C++ frontend" OFF)

if (ENABLE_GUI)
  set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE INTERNAL "Do not enable shared library")
  FetchContent_Declare(
    sdl2_library
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.5
  )
  FetchContent_GetProperties(sdl2_library)
  if (NOT sdl2_library_POPULATED)
    FetchContent_Populate(sdl2_library)
    message(STATUS "${sdl2_library_SOURCE_DIR}")
    add_subdirectory(${sdl2_library_SOURCE_DIR} ${sdl2_library_BINARY_DIR})
  endif (NOT sdl2_library_POPULATED)
  #find_package(SDL2)
endif (ENABLE_GUI)

if (SKBUILD GREATER 0)
find_package(Python COMPONENTS Interpreter Development)
else (SKBUILD GREATER 0)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
endif (SKBUILD GREATER 0)

include(FetchContent)
FetchContent_Declare(
  nanobind
  GIT_REPOSITORY https://github.com/wjakob/nanobind.git
  GIT_TAG        v1.9.2
)
FetchContent_GetProperties(nanobind)
if (NOT nanobind_POPULATED)
  FetchContent_Populate(nanobind)
  add_subdirectory(${nanobind_SOURCE_DIR} ${nanobind_BINARY_DIR})
endif (NOT nanobind_POPULATED)

nanobind_add_module(SnowfakePython SHARED
  "bindings/python/SnowfakePython.cpp"

  "src/aux_vulkan.cpp"
  "src/compute.cpp"
  "src/Simulation.cpp"
)

target_compile_definitions(SnowfakePython PUBLIC
  BUILD_PYTHON_BINDINGS
)
target_include_directories(SnowfakePython PUBLIC
  "${Python_INCLUDE_DIRS}"
  "${Vulkan_INCLUDE_DIRS}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_BINARY_DIR}"
)
target_link_libraries(SnowfakePython PUBLIC
  "${Vulkan_LIBRARIES}"
)
if (ENABLE_GUI)
  target_sources(SnowfakePython PUBLIC
    "src/renderer/PersistentGUI.cpp"
    "src/renderer/gui.cpp"
  )
  target_include_directories(SnowfakePython PUBLIC
    "${sdl2_library_SOURCE_DIR}/include"
  )
  target_link_libraries(SnowfakePython PUBLIC
    SDL2::SDL2
  )
else (ENABLE_GUI)
  target_compile_definitions(SnowfakePython PUBLIC
    NO_GUI
  )
endif (ENABLE_GUI)

add_dependencies(SnowfakePython compile_shaders_to_headers)

# The install directory is the output (wheel) directory
install(TARGETS SnowfakePython DESTINATION ${SKBUILD_PLATLIB_DIR})
