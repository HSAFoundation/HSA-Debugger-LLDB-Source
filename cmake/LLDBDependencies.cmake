set( LLDB_USED_LIBS
  lldbBase
  lldbBreakpoint
  lldbCommands
  lldbDataFormatters
  lldbHost
  lldbCore
  lldbExpression
  lldbInitialization
  lldbInterpreter
  lldbSymbol
  lldbTarget
  lldbUtility

  # Plugins
  lldbPluginDisassemblerLLVM
  lldbPluginSymbolFileDWARF
  lldbPluginSymbolFileAMDHSA
  lldbPluginSymbolFileSymtab
  lldbPluginDynamicLoaderStatic
  lldbPluginDynamicLoaderPosixDYLD
  lldbPluginDynamicLoaderHexagonDYLD
  lldbPluginDynamicLoaderWindowsDYLD
  
  lldbPluginCPlusPlusLanguage
  lldbPluginGoLanguage
  lldbPluginObjCLanguage
  lldbPluginObjCPlusPlusLanguage

  lldbPluginObjectFileELF
  lldbPluginObjectFileJIT
  lldbPluginSymbolVendorELF
  lldbPluginObjectContainerBSDArchive
  lldbPluginObjectContainerMachOArchive
  lldbPluginProcessGDBRemote
  lldbPluginProcessHSA
  lldbPluginProcessUtility
  lldbPluginPlatformAndroid
  lldbPluginPlatformGDB
  lldbPluginPlatformFreeBSD
  lldbPluginPlatformKalimba
  lldbPluginPlatformLinux
  lldbPluginPlatformNetBSD
  lldbPluginPlatformPOSIX
  lldbPluginPlatformWindows
  lldbPluginObjectContainerMachOArchive
  lldbPluginObjectContainerBSDArchive
  lldbPluginPlatformMacOSX
  lldbPluginDynamicLoaderMacOSXDYLD
  lldbPluginUnwindAssemblyInstEmulation
  lldbPluginUnwindAssemblyX86
  lldbPluginAppleObjCRuntime
  lldbPluginRenderScriptRuntime
  lldbPluginHSARuntime
  lldbPluginLanguageRuntimeGo
  lldbPluginCXXItaniumABI
  lldbPluginABIMacOSX_arm
  lldbPluginABIMacOSX_arm64
  lldbPluginABIMacOSX_i386
  lldbPluginABISysV_arm
  lldbPluginABISysV_arm64
  lldbPluginABISysV_i386
  lldbPluginABISysV_x86_64
  lldbPluginABISysV_hexagon
  lldbPluginABISysV_ppc
  lldbPluginABISysV_ppc64
  lldbPluginABISysV_mips
  lldbPluginABISysV_mips64
  lldbPluginInstructionARM
  lldbPluginInstructionARM64
  lldbPluginInstructionMIPS
  lldbPluginInstructionMIPS64
  lldbPluginObjectFilePECOFF
  lldbPluginOSGo
  lldbPluginOSPython
  lldbPluginMemoryHistoryASan
  lldbPluginInstrumentationRuntimeAddressSanitizer
  lldbPluginSystemRuntimeMacOSX
  lldbPluginProcessElfCore
  lldbPluginJITLoaderGDB
  lldbPluginExpressionParserClang
  lldbPluginExpressionParserGo
  )

# Windows-only libraries
if ( CMAKE_SYSTEM_NAME MATCHES "Windows" )
  list(APPEND LLDB_USED_LIBS
    lldbPluginProcessWindows
    lldbPluginProcessWinMiniDump
    lldbPluginProcessWindowsCommon
    Ws2_32
    Rpcrt4
    )
endif ()

# Linux-only libraries
if ( CMAKE_SYSTEM_NAME MATCHES "Linux" )
  list(APPEND LLDB_USED_LIBS
    lldbPluginProcessLinux
    lldbPluginProcessPOSIX
   )
endif ()

# FreeBSD-only libraries
if ( CMAKE_SYSTEM_NAME MATCHES "FreeBSD" )
  list(APPEND LLDB_USED_LIBS
    lldbPluginProcessFreeBSD
    lldbPluginProcessPOSIX
    )
endif ()

# NetBSD-only libraries
if ( CMAKE_SYSTEM_NAME MATCHES "NetBSD" )
  list(APPEND LLDB_USED_LIBS
    lldbPluginProcessPOSIX
    )
endif ()

# Darwin-only libraries
if ( CMAKE_SYSTEM_NAME MATCHES "Darwin" )
  list(APPEND LLDB_USED_LIBS
    lldbPluginDynamicLoaderDarwinKernel
    lldbPluginObjectFileMachO
    lldbPluginProcessMachCore
    lldbPluginProcessMacOSXKernel
    lldbPluginSymbolVendorMacOSX
    )
endif()

set( CLANG_USED_LIBS
  clangAnalysis
  clangAST
  clangBasic
  clangCodeGen
  clangDriver
  clangEdit
  clangFrontend
  clangLex
  clangParse
  clangRewrite
  clangRewriteFrontend
  clangSema
  clangSerialization
  )

set(LLDB_SYSTEM_LIBS)
if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows" AND NOT __ANDROID_NDK__)
  if (NOT LLDB_DISABLE_LIBEDIT)
    list(APPEND LLDB_SYSTEM_LIBS edit)
  endif()
  if (NOT LLDB_DISABLE_CURSES)
    list(APPEND LLDB_SYSTEM_LIBS ${CURSES_LIBRARIES})
    if(LLVM_ENABLE_TERMINFO AND HAVE_TERMINFO)
      list(APPEND LLDB_SYSTEM_LIBS ${TERMINFO_LIBS})
    endif()
  endif()
endif()
# On FreeBSD/NetBSD backtrace() is provided by libexecinfo, not libc.
if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR CMAKE_SYSTEM_NAME MATCHES "NetBSD")
  list(APPEND LLDB_SYSTEM_LIBS execinfo)
endif()

if (NOT LLDB_DISABLE_PYTHON AND NOT LLVM_BUILD_STATIC)
  list(APPEND LLDB_SYSTEM_LIBS ${PYTHON_LIBRARIES})
endif()

find_library(hwdbg_facilities_lib AMDHwDbgFacilities-x64)
if (NOT hwdbg_facilities_lib) 
  message (FATAL_ERROR "Did not find libAMDHwDbgFacilities-x64.so. You may need to add the path to the containing directory to CMAKE_PREFIX_PATH")
endif()

list(APPEND LLDB_SYSTEM_LIBS ${system_libs} ${hwdbg_facilities_lib})

if (LLVM_BUILD_STATIC)
  if (NOT LLDB_DISABLE_PYTHON)
    list(APPEND LLDB_SYSTEM_LIBS python2.7 util)
  endif()
  if (NOT LLDB_DISABLE_CURSES)
    list(APPEND LLDB_SYSTEM_LIBS gpm)
  endif()
endif()

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  interpreter
  asmparser
  bitreader
  bitwriter
  codegen
  ipo
  selectiondag
  bitreader
  mc
  mcjit
  core
  mcdisassembler
  executionengine
  runtimedyld
  option
  support
  )

if ( NOT LLDB_DISABLE_PYTHON )
  set(LLDB_WRAP_PYTHON ${LLDB_BINARY_DIR}/scripts/LLDBWrapPython.cpp)

  set_source_files_properties(${LLDB_WRAP_PYTHON} PROPERTIES GENERATED 1)
  if (CLANG_CL)
    set_source_files_properties(${LLDB_WRAP_PYTHON} PROPERTIES COMPILE_FLAGS -Wno-unused-function)
  endif()
  if (LLVM_COMPILER_IS_GCC_COMPATIBLE AND
      NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set_property(SOURCE ${LLDB_WRAP_PYTHON}
                 APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-sequence-point -Wno-cast-qual")
  endif ()
endif()
