# SnowfakePython

This is the documentation for SnowfakePython, a Python API to an implementation
of the Gravner and Griffeath 2007 method
[here](https://www.math.ucdavis.edu/~gravner/papers/h3l.pdf) for modelling snow
crystal growth (snowf(l)akes).

The crystallisation simulation is implemented on GPU through the Vulkan API,
with an optional GUI through the SDL2 library. The only dependencies the user
should have to be concerned with are the dependencies on the Vulkan SDK and the
shader compiling toolchain (such as `spirv-tools` and `shaderc`) if the
module is installed from source through `pip` or similar.

# Quickstart installation

To install, use:
```shell
$ pip install SnowfakePython@https://github.com/bjolong/SnowfakePython.git
```

then once installed, the `examples` directory contains some example Python
scripts that effect snowflake crystallisation simulation runs with known good
outcomes that roughly follow the figures in the paper.
