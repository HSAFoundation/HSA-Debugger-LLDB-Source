# HSA-Debugger-Source-AMD
The HSA-Debugger-Source-AMD repository includes the source code of the AMD GPU Debug SDK. The AMD GPU Debug SDK provides the components required to build an HSAIL kernel debugger for AMD HSA platforms.

The AMD GPU Debug SDK components are used along with the AMD HSA GDB debugger to run a gdb-based debugging environment for debugging the host application and HSAIL kernels.

# Package Contents
The AMD GPU Debug SDK includes the source code and libraries briefly listed below
- Source Code 
  - AMD HSA Debug Agent: The AMD HSA Debug Agent is a library injected into an HSA application by the AMD HSA runtime. The source code for the Agent is provided in *src/HSADebugAgent*.
  - AMD Debug Facilities: The AMD Debug Facilities is a utility library to perform symbol processing for AMD HSA code object.  The source code is provided in *src/HwDbgFacilities*.
  - Matrix multiplication example: A sample HSA application that runs a matrix multiplication kernel.
- Header files and libraries
  - libAMDGPUDebugHSA-x64: This library provides the low level hardware control required to enable debugging a kernel executing on an HSA platform. The functionality of this library is exposed by the header file *AMDGPUDebug.h*  in *include/*. The HSA Debug Agent library uses this interface
   - libelf: A libelf library compatible with the AMD HSA software stack and its corresponding header files. The HSA Debug Agent library uses this libelf.
	
# Build Steps
1. Set up HSA Dependencies
  * [Install HSA Driver](https://github.com/HSAFoundation/HSA-Drivers-Linux-AMD#installing-and-configuring-the-kernel)
  * [Install HSA Runtime](https://github.com/HSAFoundation/HSA-Runtime-AMD/#installing-and-configuring-the-hsa-runtime)
2. Clone the Debug SDK repository
  * `git clone https://github.com/HSAFoundation/HSA-Debugger-Source-AMD.git`
3. Build the AMD HSA Debug Agent Library and the Matrix multiplication examples by calling *make* in the *src/HSADebugAgent* and the *samples/MatrixMultiplication* directories respectively
  * `cd src/HSADebugAgent`
  * `make`
    * Note that *matrixMul_kernel.hsail* is included for reference only. This sample will load the pre-built hsa binary (*matrixMul_kernel.brig*) to run the kernel.
  * `cd samples/MatrixMultiplication`
  * `make`
4. Build the AMD GDB debugger [as shown in the GDB repository](https://github.com/HSAFoundation/HSA-Debugger-GDB-Source-AMD).