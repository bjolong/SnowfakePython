# Welcome to SnowfakePython

This is the documentation for SnowfakePython, a Python API to an implementation
of the Gravner and Griffeath 2007 method for modelling snow crystal growth
(snowflakes).

The crystallisation simulation is implemented on GPU through the Vulkan API,
with an optional GUI through the SDL2 library. The only dependencies the user
should have to be concerned with are the dependencies on the Vulkan SDK and the
shader compiling toolchain (such as `spirv-tools` and `shaderc`) if the
module is installed from source through `pip` or similar.

readme = "README.md"
keywords = ["snowflake", "crystallization", "simulation"]

## Commands

* `mkdocs new [dir-name]` - Create a new project.
* `mkdocs serve` - Start the live-reloading docs server.
* `mkdocs build` - Build the documentation site.
* `mkdocs -h` - Print help message and exit.

## Project layout

    mkdocs.yml    # The configuration file.
    docs/
        index.md  # The documentation homepage.
        ...       # Other markdown pages, images and other files.
