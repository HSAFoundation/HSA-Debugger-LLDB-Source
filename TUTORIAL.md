Building a debuggable binary
----------------------------

Binaries can be built using [HSAILAsm](https://github.com/HSAFoundation/HSAIL-Tools) or [CLOC](https://github.com/HSAFoundation/CLOC).

To build a debuggable binary with HSAILasm, you need to supply the `-g -include-source` flags.

CLOC can produce binaries with debug information for debugging HSAIL or OpenCL C.
For debugging HSAIL, execute the following command before building the application:

    export LIBHSAIL_OPTIONS_APPEND="-g -include-source"

For debugging OpenCL C, call cloc.sh with the following flag:

    -clopts "-g"

OpenCL C debugging was tested with CLOC v0.9 and is rather unstable compared to HSAIL debugging.

Running HSA LLDB
----------------

LLDB should be launched using the provided hsail-lldb script. Ensure that the folders containing libAMDHwDbgFacilities-x64.so, libAMDHSADebugAgent-x64 and the runtime libraries will be searched by the dynamic loader. You may need to add the paths to LD_LIBRARY_PATH.

If the debugger does not work, ensure that you are using the version of HSADebugAgent supplied with HSA LLDB, not the one from the external HSA-Debugger-Source-AMD repository.

Launching the executable
------------------------

To launch an executable which uses the HSA runtime, just use the standard LLDB commands:

    file MatrixMul
    run

If everything is okay, you should see the kernel launching and completing.

Setting an HSAIL kernel breakpoint
---------------------------

HSAIL kernel breakpoints are set using the following command:

    language hsa breakpoint set -k<kernel name> [-l<line-number>]

You can use abbreviations, like any other LLDB command

    lang hsa break set -k&__OpenCL_matrixMul_kernel
    la h b s -k&__OpenCL_matrixMul_kernel -l72

Kernel breakpoints can be set automatically without knowing the name of the kernel in advance. This is toggled with the following command:

    language hsa breakpoint all enable
    language hsa breakpoint all disable

Currently, the kernel name breakpoints can be set before the application has even been launched, but the toggle must be set post-launch. This will be fixed in the future, but for now you can just set a breakpoint on `main`, toggle kernel breakpoints and continue:

    break set -n main
    r
    lang hsa break all enable
    c

Continuing from a breakpoint
----------------------------

Once a breakpoint has been hit, you can continue execution using the standard LLDB command

    continue
    c

Manipulating breakpoints
------------------------

HSAIL kernel breakpoints can be disabled, enabled and removed like any other LLDB breakpoint:

    break list

    1: HSA kernel breakpoint for '&__OpenCL_matrixMul_kernel', locations = 1, resolved = 1, hit count = 39
      1.1: address = 0x00000000000002a0, resolved, hit count = 39

    break disable 1
    break enable 1
    break delete 1

Single-stepping
---------------

You can execute a source-level step-over while stopped in an HSAIL kernel.

    thread step-over
    next
    n

Step-in and step-out are not currently supported.

Threading model
---------------

Currently, each thread which is displayed by LLDB is a single wavefront. Soon there will be a better way of selecting the active wavefront by supplying the relevant identifiers.

Reading HSAIL registers, kernel arguments and OpenCL C variables
----------------------------------------------------------------

Registers, kernel arguments and OpenCL C variables can be read using the following command:

    language hsa variable read -v <name> [-x<work-item x> -y<work-item y> -z<work-item z>]

