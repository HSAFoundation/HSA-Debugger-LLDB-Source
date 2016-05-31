//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Header for RunAgentCommandLoop
//==============================================================================
#ifndef _COMMANDLOOP_H_
#define _COMMANDLOOP_H_

#include "AgentContext.h"
#include "CommunicationControl.h"

namespace HwDbgAgent
{
/// Struct to pass parameters to the DBE Thread
typedef struct
{
    /// This might be needed by the DBE thread to stop the dispatch
    /// first and then itself to signal GDB
    pid_t m_dispatchPID;

    /// This is got from the predispatch callback argument and passed to the DebugThread
    AgentContext* m_pHsailAgentContext;

} DebugEventThreadParams ;

/// Read the FIFO and check for any new packets
void RunFifoCommandLoop(AgentContext* pActiveContext);

/// Read the FIFO and check for any new packets multiple times
void RunFifoCommandLoop(AgentContext* pActiveContext, unsigned int runCount);

HsailAgentStatus WaitForDebugThreadCompletion();

HsailAgentStatus CreateDebugEventThread(DebugEventThreadParams* pArgs);

} // End Namespace HwDbgAgent

#endif // _COMMANDLOOP_H_
