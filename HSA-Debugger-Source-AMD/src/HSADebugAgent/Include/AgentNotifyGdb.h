//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Initial functionality to notify gdb about events that the DBE / agent may see
//==============================================================================
#ifndef _AGENT_NOTIFY_H_
#define _AGENT_NOTIFY_H_

#include "AMDGPUDebug.h"
#include "CommunicationControl.h"

// Initialization notification, sends a SIGALRM to gdb.
// this is different from the other notifications written to the FIFO
void AgentNotifyGDB();

/// Trigger the GDB event loop
void AgentTriggerGDBEventLoop();

HsailAgentStatus AgentNotifyBreakpointHit(const HsailNotificationPayload payload);

/// Let GDB know about a new binary, the notification sends parameters for the binary
/// which will be found in shared memory
/// This will also update dispatch statistics. Note that this logic will break if
/// the GDB is notified multiple times with for the same binary or
/// 1 binary doesn't describe one dispatch sufficiently in the future.
HsailAgentStatus AgentNotifyNewBinary(const size_t          binarySize,
                                      const std::string&    hlSymbolName,
                                      const std::string&    llSymbolName,
                                      const std::string&    kernelName,
                                      const HsailWaveDim3&  workGroupSize,
                                      const HsailWaveDim3&  gridSize);

/// Let GDB know about a change in the focus workgroup and workitem
HsailAgentStatus AgentNotifyFocusChange(const HwDbgDim3& focusWorkGroup,
                                        const HwDbgDim3& focusWorkItem);

/// Let GDB know that the dispatch has been killed
HsailAgentStatus AgentNotifyKillComplete(const bool isKillSuccess, const bool isQuitCommandIssued);

/// Let GDB know about begin debugging, it will change how GDB prints signals and steps
HsailAgentStatus AgentNotifyBeginDebugging(const bool setDeviceFocus);

/// Let GDB know about end debugging, if the dispatch is completed the binary sent will be discarded
HsailAgentStatus AgentNotifyEndDebugging(const bool hasDispatchCompleted);

/// Let GDB know if the application is in the predispatch callback
HsailAgentStatus AgentNotifyPredispatchState(const HsailPredispatchState ipState);

/// Let GDB know about the debug threads ID. We use the debug thread ID to single step accordingly
HsailAgentStatus AgentNotifyDebugThreadID();

#endif // _AGENTNOTIFY_H_
