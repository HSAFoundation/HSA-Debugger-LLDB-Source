//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Implementation of the Agent context
//==============================================================================
#ifndef _AGENT_CONTEXT_H_
#define _AGENT_CONTEXT_H_

#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>

// HSA headers
#include <hsa.h>

// DBE Headers
#include "AMDGPUDebug.h"

//Agent headers
#include "AgentLogging.h"
#include "CommunicationControl.h"

namespace HwDbgAgent
{
class AgentBinary;
class AgentBreakpointManager;
class AgentFocusWaveControl;
class AgentWavePrinter;

typedef enum
{
    HSAIL_PARENT_STATUS_UNKNOWN,        /// Parent status is unknown
    HSAIL_PARENT_STATUS_GOOD,           /// The getppid() function OP matches the saved ppid
    HSAIL_PARENT_STATUS_TERMINATED,     /// getppid() function does not match the saved ppid
    HSAIL_PARENT_STATUS_CHECK_COUNT_MAX /// This check happened too many times
} HsailParentStatus;

static const HwDbgDim3 g_UNKNOWN_HWDBGDIM3 = {uint32_t(-1), uint32_t(-1), uint32_t(-1)};

/// The AgentContext includes functionality to start and stop debug.
/// The AgentContext is initialized when the agent is loaded and passed as the UserArg field
/// to the predispatch callback function.
/// It is deleted when the agent is unloaded.
class AgentContext
{
private:

    /// This enum is private since we don't want others to change the AgentContext state
    typedef enum _HsailAgentState
    {
        HSAIL_AGENT_STATE_UNKNOWN,          /// We havent set it yet
        HSAIL_AGENT_STATE_OPEN,             /// The agent is open, all initialization steps are complete
        HSAIL_AGENT_STATE_BEGIN_DEBUGGING,  /// HwDBg has started
        HSAIL_AGENT_STATE_END_DEBUGGING,    /// HwDbg has ended (We are not bothering with adding post breakpoint here)
        HSAIL_AGENT_STATE_CLOSED            /// The agent has been closed by GDB

    } HsailAgentState;

    std::vector<AgentBinary*> m_pKernelBinaries;

    /// Enum that describes the state of the agent
    HsailAgentState m_AgentState ;

    /// Input passed to the DBE
    HwDbgState m_HwDebugState;

    /// This parameter is passed to the DBE with every call
    HwDbgContextHandle m_DebugContextHandle;

    /// The last DBE event type
    HwDbgEventType m_LastEventType;

    /// The parent process ID
    int m_ParentPID;

    /// Disable copy constructor
    AgentContext(const AgentContext&);

    /// Disable assignment operator
    AgentContext& operator=(const AgentContext&);

    /// Private function to begin debugging
    HsailAgentStatus BeginDebugging();

    /// Release the latest kernel binary we have.
    /// Called when we do EndDebug with HWDBG_BEHAVIOR_NONE or when we register any new binary
    HsailAgentStatus ReleaseKernelBinary();

    /// These functions are called only once irrespective of how many ever binaries
    /// are encountered by the application. Thats why this func is part of the context
    /// rather than the AgentBinary class.
    /// The key is not passed since its global
    /// Private function to initialize the shared memory buffer for the binary
    HsailAgentStatus AllocateBinarySharedMemBuffer();

    /// Private function to free the shared memory buffer for the binary
    HsailAgentStatus FreeBinarySharedMemBuffer();

public:
    /// A bit to track that we have received the continue command from the host
    bool m_ReadyToContinue;

    /// The active dispatch dimensions populated from the Aqlpacket when begin debug
    HwDbgDim3 m_workGroupSize;

    /// The active dispatch dimensions populated from the Aqlpacket when begin debug
    HwDbgDim3 m_gridSize;

    /// The breakpoint manager we use for this context
    AgentBreakpointManager* m_pBPManager;

    /// The wave printer we use for this context
    AgentWavePrinter* m_pWavePrinter;

    /// The focus wave control we use for this context
    AgentFocusWaveControl* m_pFocusWaveControl;

    AgentContext():
        m_AgentState(HSAIL_AGENT_STATE_UNKNOWN),// State is unknown initially
        m_HwDebugState(),                       // This will zero initialize the structure
        m_DebugContextHandle(nullptr),             // No debug context is known
        m_LastEventType(HWDBG_EVENT_INVALID),
        m_ParentPID(getppid()),
        m_ReadyToContinue(false),
        m_workGroupSize(g_UNKNOWN_HWDBGDIM3),
        m_gridSize(g_UNKNOWN_HWDBGDIM3),
        m_pBPManager(nullptr),
        m_pWavePrinter(nullptr),
        m_pFocusWaveControl(nullptr)
    {
        AGENT_LOG("Constructor Agent Context");
    }

    /// Destructor that shuts down the AgentContext if not already shut down.
    ~AgentContext();

    /// This function is called when the agent is loaded and the handshake
    /// with GDB is complete
    /// It only captures that the AgentContext object in now in a HSAIL_AGENT_STATE_OPEN
    /// state and debugging has not yet started.
    HsailAgentStatus Initialize();

    /// Shutdown API, will be called in destructor if not called explicitly
    /// If true, we skip the DBE shutdown call. That way we can call this function
    /// in the Unload too
    HsailAgentStatus ShutDown(const bool skipDbeShutDown);

    /// Begin debugging
    /// Assumes only one session active at a time
    /// This function takes individual HSA specific parameters and then populates
    /// the HwDbgState
    HsailAgentStatus BeginDebugging(const hsa_agent_t                   agent,
                                    const hsa_queue_t*                  pQueue,
                                          hsa_kernel_dispatch_packet_t* pAqlPacket,
                                          uint32_t                      behaviorFlags);

    /// Resume debugging, does all the state checks internally
    HsailAgentStatus ContinueDebugging();

    /// Force complete dispatch
    HsailAgentStatus ForceCompleteDispatch();

    /// End debugging, does not close the agent
    HsailAgentStatus EndDebugging(const bool& forceCleanUp = false);

    /// Kill all waves
    HsailAgentStatus KillDispatch(const bool isQuitIssued);

    /// Save the code object in the AgentContext
    HsailAgentStatus AddKernelBinaryToContext(AgentBinary* pAgentBinary);

    /// The wrapper around the DBE's function
    HsailAgentStatus WaitForEvent(HwDbgEventType* pEventTypeOut);

    /// Accessor method to return the active context, needed for things like Breakpoints
    const HwDbgContextHandle GetActiveHwDebugContext() const;

    /// Just a logging function
    const std::string GetAgentStateString() const;

    /// Accessor method to get the BP manager for this context
    AgentBreakpointManager* GetBpManager() const;

    /// Accessor method to return the wave printer for this context
    AgentWavePrinter* GetWavePrinter() const;

    /// Accessor method to return the focus wave controller for this context
    AgentFocusWaveControl* GetFocusWaveControl() const;

    /// Set the Agent and DBE logging
    HsailAgentStatus SetLogging(const HsailLogCommand loggingConfig)const;

    /// Return true if HwDebug has started
    bool HasHwDebugStarted() const;

    /// Just a logging function
    HsailAgentStatus PrintDBEVersion() const;

    // Raise trap signal
    HsailAgentStatus RaiseTrapSignal() const;

    /// Raise stop signal
    HsailAgentStatus RaiseStopSignal() const;

    /// Compare parent PID saved at object creation with present parent PID
    bool CompareParentPID() const;
};

} // End Namespace HwDbgAgent

#endif // _AGENT_CONTEXT_H_
