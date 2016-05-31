include_directories(.)

add_lldb_library(lldbPluginProcessHSA
  RegisterContextHSA.cpp
  NativeRegisterContextHSA.cpp
  UnwindHSA.cpp
  NativeBreakpointHSA.cpp
  NativeThreadHSA.cpp
  ThreadPlanStepOverHSA.cpp
  )
