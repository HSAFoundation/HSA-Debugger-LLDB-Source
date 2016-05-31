HSA-Debugger-LLDB-Source
------------------------

This repository contains the source code for a prototype HSA-enabled fork of LLDB. It provides an LLDB-based debugging environment for debugging both the host application and GPU kernels running on AMD HSA hardware. The road-map for the project is to transition to a future standardized multi-vendor HSA debugging API, when it is released publicly. Kernels can be debugged if they are developed with the LLVM open-source compilers that emit DWARF debugging information in the BRIG output.

Features
--------
- HSAIL or OpenCL C kernel debugging
- Source-level single-stepping
- Can be integrated with other LLVM projects
- Kernel entry and kernel source line breakpoints
- Ability to read HSAIL registers, kernel arguments and OpenCL C variables

Contents
--------
This repository contains

- LLDB source code, with HSA features added
- HSA debug agent component for LLDB (the plan is to integrate this with the gdb debug agent for HSA)


Hardware
--------
This debugger was written for the AMD Kaveri APU.

Building
--------
Install the HSA runtime and drivers from [here](https://github.com/HSAFoundation/). This debugger was written and tested against the September 2015 releases of these components. 

Build the HSA debug facilities library from [here](https://github.com/HSAFoundation/HSA-Debugger-Source-AMD). Do not use the HSADebugAgent from that repository; there is an lldb-specific one in this repository.

Get LLVM and Clang 3.8 source code from [here](http://llvm.org/releases/download.html#3.8.0).

Build the supplied HSA debug agent component in the HSA-Debugger-Source-AMD folder in this repository.

Build HSA LLDB using the [usual LLDB instructions](http://lldb.llvm.org/build.html). If the HwDbgFacilities library is not installed in a location searched by your linker, you may need to add the path to it to the CMAKE_PREFIX_PATH environment variable when running cmake.


Usage
-----
See [this](https://github.com/HSAFoundation/HSA-Debugger-LLDB-Source/blob/master/TUTORIAL.md) file for a brief tutorial on how to debug HSA applications.

Samples
-------
There are two sample programs in the hsa_samples folder:

- MatrixMultiplication - HSAIL debugging
- vector_copy - OpenCL C debugging

These samples have pre-built brig included, so you don't need HSAILasm or CLOC to test them, but the sources are there in case you want to build them yourself.