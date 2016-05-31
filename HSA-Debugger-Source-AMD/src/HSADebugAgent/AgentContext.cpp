//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Implementation of the Agent context
//==============================================================================
#include <cstdint>

#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>

#include "AMDGPUDebug.h"
#include "AgentBinary.h"
#include "AgentBreakpointManager.h"
#include "AgentContext.h"
#include "AgentFocusWaveControl.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentUtils.h"
#include "AgentWavePrinter.h"
#include "CommunicationControl.h"
#include "CommandLoop.h"

namespace HwDbgAgent
{

HsailAgentStatus AgentContext::AllocateBinarySharedMemBuffer()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    status = AgentAllocSharedMemBuffer(g_DBEBINARY_SHMKEY, g_BINARY_BUFFER_MAXSIZE);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("AllocateBinarySharedMemBuffer: Could not alloc shared memory");
    }

    return status;
}

//! Private function to initialize the shared memory buffer for the binary
HsailAgentStatus AgentContext::FreeBinarySharedMemBuffer()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    status = AgentFreeSharedMemBuffer(g_DBEBINARY_SHMKEY, g_BINARY_BUFFER_MAXSIZE);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("FreeBinarySharedMemBuffer: Could not free binary shared mem");
    }

    return status;
}

// This mechanism allows us to delete the binary object when we get to EndDebugging
HsailAgentStatus AgentContext::AddKernelBinaryToContext(AgentBinary* pAgentBinary)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_SUCCESS;

    // We need to release what we had previously if we are getting a new one
    status = ReleaseKernelBinary();

    m_pKernelBinaries.push_back(pAgentBinary);
    return status;
}

// Start Debugging, set up a HwDbgState struct and call the DBE
HsailAgentStatus AgentContext::BeginDebugging(const hsa_agent_t                   agent,
                                              const hsa_queue_t*                  pQueue,
                                                    hsa_kernel_dispatch_packet_t* pAqlPacket,
                                              const uint32_t                      behaviorFlags)
{

    if (pQueue == nullptr)
    {
        AGENT_ERROR("BeginDebugging: pQueue is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    if (pAqlPacket == nullptr)
    {
        AGENT_ERROR("BeginDebugging: pAqlPacket is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    memset(&m_HwDebugState, 0, sizeof(HwDbgState));

    // HSA: set to hsa_agent_t (from pre-dispatch callback)
    m_HwDebugState.pDevice = reinterpret_cast<void*>(agent.handle);

    // HSA: set to hsa_kernel_dispatch_packet_t* (from pre-dispatch callback)
    m_HwDebugState.pPacket = pAqlPacket;
    m_HwDebugState.behaviorFlags = behaviorFlags;

    m_workGroupSize.x = pAqlPacket->workgroup_size_x;
    m_workGroupSize.y = pAqlPacket->workgroup_size_y;
    m_workGroupSize.z = pAqlPacket->workgroup_size_z;

    m_gridSize.x = pAqlPacket->grid_size_x;
    m_gridSize.y = pAqlPacket->grid_size_y;
    m_gridSize.z = pAqlPacket->grid_size_z;


    AGENT_LOG("Behavior Flag: " << behaviorFlags << "\t"
              << "Workgroup dimensions " << m_workGroupSize.x << " "
              << m_workGroupSize.y << " " << m_workGroupSize.z << "\t"
              << "Grid dimensions " << m_gridSize.x << " "
              << m_gridSize.y << " " << m_gridSize.z);

    // HSA: set to packet_id (from pre-dispatch callback --- not exist yet)
    //m_HwDebugState.packetId =;

    return BeginDebugging();

}

// Start Debugging. Use the member HwDbgState object and call the DBE
HsailAgentStatus AgentContext::BeginDebugging()
{
    // The member HwDbgState member object should have been set up and validated already
    // Also this function is private so it cant be called anywhere else
    // Just double-check a couple of parameters
    if (m_HwDebugState.pDevice == nullptr)
    {
        AGENT_ERROR("BeginDebugging: pDevice is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    if (m_HwDebugState.pPacket == nullptr)
    {
        AGENT_ERROR("BeginDebugging: pPacket is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // We cannot start debugging when it has already started
    if (m_AgentState == HSAIL_AGENT_STATE_BEGIN_DEBUGGING)
    {
        AGENT_ERROR("BeginDebugging: HwDbg has already been started");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    // Call the DBE and save the Context handle
    HwDbgStatus dbeStatus = HwDbgBeginDebugContext(m_HwDebugState, &m_DebugContextHandle);
    assert(dbeStatus == HWDBG_STATUS_SUCCESS);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR(GetDBEStatusString(dbeStatus));
        return status;
    }
    else
    {
        AGENT_LOG("BeginDebugging: Started HwDbg");
        status = AgentNotifyBeginDebugging(true);
    }

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("BeginDebugging: Could not notify GDB");
    }

    m_AgentState = HSAIL_AGENT_STATE_BEGIN_DEBUGGING;

    return status;
}

HsailAgentStatus AgentContext::ContinueDebugging()
{
    // We cannot do Continue in any other state
    if (m_AgentState != HSAIL_AGENT_STATE_BEGIN_DEBUGGING)
    {
        AGENT_ERROR("ContinueDebugging: Cannot call Continue without BeginDebugging ");

        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // Check if the handle is nullptr
    if (m_DebugContextHandle == nullptr)
    {
        AGENT_ERROR("ContinueDebugging: context handle is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // This is needed to catch the case of doing a continue at a function breakpoint
    // The problem is that the focus has been switched to HSAIL
    // So gdb sends a hsail-continue packet.
    //
    // However waitforevent has not been called and a kernel breakpoint has not been hit
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (m_LastEventType == HWDBG_EVENT_POST_BREAKPOINT)
    {
        HwDbgStatus dbeStatus = HwDbgContinueEvent(m_DebugContextHandle, HWDBG_COMMAND_CONTINUE);

        if (dbeStatus != HWDBG_STATUS_SUCCESS)
        {
            AGENT_ERROR("ContinueDebugging: Error from DBE");
            assert(dbeStatus == HWDBG_STATUS_SUCCESS);
            status = HSAIL_AGENT_STATUS_FAILURE;
        }
        else
        {
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
    }
    else
    {
        if (m_LastEventType == HWDBG_EVENT_END_DEBUGGING)
        {
            AGENT_ERROR("ContinueDebugging: Continue should not be called after EndDebugging");
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
        else if (m_LastEventType == HWDBG_EVENT_TIMEOUT)
        {
            AGENT_LOG("Continue is being recalled after Debug Event Timeout");
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    m_ReadyToContinue = true;
    return status;
}

HsailAgentStatus AgentContext::ForceCompleteDispatch()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    HwDbgStatus dbeStatus;

    if (m_DebugContextHandle == nullptr)
    {
        AGENT_ERROR("ForceCompleteDispatch: DBE Handle is nullptr ");
        return status;
    }

    status = m_pBPManager->DisableAllBreakpoints(m_DebugContextHandle);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR ("ForceCompleteDispatch: Could not disable all existing breakpoints");
    }

    // Do a wait - continue loop to try and complete the dispatch.
    // We could try to Kill the dispatch if this fails but that could mess up correctness
    int loopCount = 0;
    const int MAX_LOOP_COUNT = 20;
    for ( loopCount= 0; loopCount <= MAX_LOOP_COUNT; loopCount++)
    {
        // Sleep for 1ms
        usleep(1000);

        AGENT_LOG("ForceCompleteDispatch: Wait-Continue Iteration # " << loopCount << " out of "<< MAX_LOOP_COUNT);

        dbeStatus = HwDbgContinueEvent(m_DebugContextHandle, HWDBG_COMMAND_CONTINUE);
        if (dbeStatus != HWDBG_STATUS_SUCCESS)
        {
            AGENT_ERROR("ForceCompleteDispatch: Error in HwDbgWaitForEvent "
                        << GetDBEStatusString(dbeStatus));
        }

        HwDbgEventType eventType = HWDBG_EVENT_INVALID;
        dbeStatus = HwDbgWaitForEvent(m_DebugContextHandle, 10, &eventType);
        if (dbeStatus != HWDBG_STATUS_SUCCESS)
        {
            AGENT_ERROR("ForceCompleteDispatch: Error in HwDbgWaitForEvent "
                        << GetDBEStatusString(dbeStatus));
        }
        AGENT_LOG("ForceCompleteDispatch: DBE Event type " << GetDBEEventString(eventType));
        if (eventType == HWDBG_EVENT_END_DEBUGGING )
        {
            break;
        }
    }

    // Now attempt to end debugging (forcefully)
    status = EndDebugging(false);
    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("ForceCompleteDispatch: Could not end debugging");
    }

    return status;
}

HsailAgentStatus AgentContext::EndDebugging(const bool& forceCleanup)
{

    // Check if the handle is nullptr
    if (m_DebugContextHandle == nullptr)
    {
        AGENT_ERROR("EndDebugging: context handle is nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    if (m_AgentState != HSAIL_AGENT_STATE_BEGIN_DEBUGGING)
    {
        AGENT_ERROR("EndDebugging: Cannot end debugging without BeginDebugging");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    HsailAgentStatus agentStatus = HSAIL_AGENT_STATUS_FAILURE;

#if 0
    // HSADBG-363
    // Disabled while debugging
    //
    // We should now disable all the breakpoints by deleting them in the DBE
    // before we call end debugging.
    // Ideally, this should be done after EndDebugging but then the DBE handle is invalid
    agentStatus = m_pBPManager->DisableAllBreakpoints(m_DebugContextHandle);

    if (HSAIL_AGENT_STATUS_SUCCESS != agentStatus)
    {
        AGENT_ERROR("EndDbugging: Could not disable all breakpoints");
    }
#endif

    HwDbgStatus status;
    if (forceCleanup)
    {
        status = HwDbgEndDebugContext(nullptr);
    }
    else
    {
        // Verify that the fifo is empty  is done in the commandLoop()
        status = HwDbgEndDebugContext(m_DebugContextHandle);
    }

    if (status != HWDBG_STATUS_SUCCESS && status != HWDBG_STATUS_UNDEFINED)
    {
        AGENT_ERROR("EndDebugging: Error in EndDebugging " <<
                    GetDBEStatusString(status));

        return agentStatus;
    }


    m_DebugContextHandle =  nullptr;

    m_AgentState = HSAIL_AGENT_STATE_END_DEBUGGING;

    if (m_LastEventType == HWDBG_EVENT_END_DEBUGGING &&
        m_HwDebugState.behaviorFlags == HWDBG_BEHAVIOR_NONE)
    {
        agentStatus = AgentNotifyEndDebugging(true);
    }
    else
    {
        agentStatus = AgentNotifyEndDebugging(false);
    }

    m_ReadyToContinue = false;
    if (HSAIL_AGENT_STATUS_SUCCESS != agentStatus)
    {
        AGENT_ERROR("Could not notify GDB of EndDebugging");
        return agentStatus;
    }

    switch (m_HwDebugState.behaviorFlags )
    {
    case HWDBG_BEHAVIOR_NONE:
        agentStatus = ReleaseKernelBinary();
        break;

    case HWDBG_BEHAVIOR_DISABLE_DISPATCH_DEBUGGING:
        AGENT_LOG("EndDebugging: Don't delete the binary since GDB may use it later")
        break;
    default:
        break;
    }

    return agentStatus;
}

// Kill the ongoing dispatch,
// If you are exiting HwDbg, EndDebugging needs to be called separately
HsailAgentStatus AgentContext::KillDispatch(const bool isQuitIssued)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (!HasHwDebugStarted())
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
        return status;
    }

    HwDbgStatus dbeStatus;
    // Do 10 iterations just to be safe
    bool isDispatchKilled = false;
    int count = 0;

    while (count < 10)
    {
        dbeStatus = HwDbgKillAll(m_DebugContextHandle);

        // KillAll can return an error if
        // Max no of wavecontrol kills have been done and the dispatch didnt complete
        // so we try again if it is an error
        if (dbeStatus == HWDBG_STATUS_SUCCESS)
        {
            isDispatchKilled = true;
            status = HSAIL_AGENT_STATUS_SUCCESS;
            break;
        }
    }

    if (isDispatchKilled)
    {
        AgentNotifyKillComplete(isDispatchKilled, isQuitIssued);
    }
    else
    {
        AGENT_ERROR("KillDispatch: Error in HwDbgKillAll, tried 10 times" <<
                    GetDBEStatusString(dbeStatus));
        status = HSAIL_AGENT_STATUS_FAILURE;
    }

    return status;
}

// To return whether HwDebugBeginDebugging has been called  or not
// Return true if HwDbgBeginDebugging has been called
bool AgentContext::HasHwDebugStarted() const
{
    if (m_AgentState == HSAIL_AGENT_STATE_BEGIN_DEBUGGING)
    {
        // Check if HwDebugBeginDebugging has been called
        return true;
    }
    else
    {
        // This may not be an error case
        AGENT_ERROR("HasHwDebugStarted: Agent not in Begin Debugging " <<
                    GetAgentStateString());

        return false;
    }

    return false;

}

/// Return the active DBE context
/// This is the method, needed to pass a DBE context for other objects such as the breakpoint manager
/// nullptr returned if HwDbg has not sarted. The caller needs to check
const HwDbgContextHandle AgentContext::GetActiveHwDebugContext() const
{
    // These checks are disabled since a nullptr context is valid
    // when HwDbg has not yet started

#if 0
    if (m_DebugContextHandle == nullptr)
    {
        AGENT_ERROR("GetActiveHwDebugContext: Returning a nullptr HwDebugContext");
    }

    if (m_AgentState != HSAIL_AGENT_STATE_BEGIN_DEBUGGING)
    {
        AGENT_ERROR("GetActiveHwDebugContext: Agent not in Begin Debugging");
    }

#endif
    return m_DebugContextHandle;
}

// This is used by all the calling functions to create and delete breakpoints
AgentBreakpointManager* AgentContext::GetBpManager() const
{
    if (m_pBPManager == nullptr)
    {
        AGENT_ERROR("GetBpManager: Returning a nullptr breakpoint manager");
    }


    return m_pBPManager;
}

// This is used by all the calling functions to create and delete breakpoints
AgentWavePrinter* AgentContext::GetWavePrinter() const
{
    if (m_pWavePrinter == nullptr)
    {
        AGENT_ERROR("GetWavePrinter: Returning a nullptr breakpoint manager");
    }

    return m_pWavePrinter;
}

// This is used by all the calling functions to control the focus wave and notify gdb
AgentFocusWaveControl* AgentContext::GetFocusWaveControl() const
{
    if (m_pFocusWaveControl == nullptr)
    {
        AGENT_ERROR("GetFocusWaveControl: Returning a nullptr FocusWaveControl");
    }

    return m_pFocusWaveControl;
}


// Called once the object has been created
// Explicitly done rather than moving this into the constructor since we want to be sure
// We will also initialize the breakpoint manager in this case
HsailAgentStatus AgentContext::Initialize()
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (m_AgentState != HSAIL_AGENT_STATE_UNKNOWN)
    {
        AGENT_ERROR("Initialize: Attempting to initialize AgentContext multiple times");
        return status;
    }

    if (m_pBPManager != nullptr || m_pWavePrinter != nullptr)
    {
        AGENT_ERROR("BP manager is already initialized");
        return status;
    }

    status =  AllocateBinarySharedMemBuffer();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not allocate the shared memory for the DBE binary");
        return status;
    }

    // Initialize a breakpoint manager
    m_pBPManager = new(std::nothrow) AgentBreakpointManager;

    m_pWavePrinter = new(std::nothrow) AgentWavePrinter;

    m_pFocusWaveControl = new(std::nothrow) AgentFocusWaveControl;

    if (m_pBPManager == nullptr || m_pWavePrinter == nullptr || m_pFocusWaveControl == nullptr)
    {
        AGENT_ERROR("Could not initialize a BP manager or a wave printer");

        status = HSAIL_AGENT_STATUS_FAILURE;
    }
    else
    {
        if (m_AgentState != HSAIL_AGENT_STATE_UNKNOWN)
        {
            AGENT_ERROR("The agent has already been initialized");

            status = HSAIL_AGENT_STATUS_FAILURE;
        }
        else
        {
            // We have initialized a breakpoint manager successfully
            // and the Initialize function has only been called once
            m_AgentState = HSAIL_AGENT_STATE_OPEN;

            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    return status;
}

HsailAgentStatus AgentContext::PrintDBEVersion() const
{
    unsigned int versionMajorOut;
    unsigned int versionMinorOut;
    unsigned int versionBuildOut;

    HwDbgStatus status =  HwDbgGetAPIVersion(&versionMajorOut,
                                             &versionMinorOut,
                                             &versionBuildOut);

    assert(status == HWDBG_STATUS_SUCCESS);

    if (status != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("PrintDBEVersion: Error getting API Information");
        return HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        AGENT_OP("AMD DBE Version " << versionMajorOut
                 << "." << versionMinorOut << "." << versionBuildOut);

        return HSAIL_AGENT_STATUS_FAILURE;
    }
}

HsailAgentStatus AgentContext::RaiseTrapSignal() const
{
  if (kill(getpid(), SIGTRAP) == -1)
    {
        // I am not sure on how to handle signaling failure
        AGENT_ERROR("Could not signal gdb");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    return HSAIL_AGENT_STATUS_SUCCESS;
}



HsailAgentStatus AgentContext::RaiseStopSignal() const
{
    if (pthread_kill(pthread_self(), SIGUSR2) == -1)
    {
        // I am not sure on how to handle signaling failure
        AGENT_ERROR("Could not signal gdb");
        return HSAIL_AGENT_STATUS_FAILURE;
    }
    else
    {
        AGENT_LOG("RaiseStopSignal: Raise SIGUSR2");
    }


    return HSAIL_AGENT_STATUS_SUCCESS;
}

HsailAgentStatus AgentContext::ReleaseKernelBinary()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_SUCCESS;

    // Delete the last added binary, it is possible to have nothing left over since
    // since this function can be called when we first add a binary too
    if (0 >= m_pKernelBinaries.size())
    {
        // We can have 0  binaries if the dispatch did debug since no function
        // breakpoints matched up
        AGENT_LOG("ReleaseKernelBinary: The context does not have any binary presently");
        return status;
    }

    AgentBinary* pBinary = m_pKernelBinaries.at(m_pKernelBinaries.size() - 1);
    if (pBinary != nullptr)
    {
        delete pBinary;
    }
    else
    {
        AGENT_ERROR("ReleaseKernelBinary: Releasing binaries, A nullptr binary exists in m_pKernelBinaries");
        status = HSAIL_AGENT_STATUS_FAILURE;
    }

    m_pKernelBinaries.pop_back();
    return status;
}

// This controls the logging from the GDB side
// Agent side logging from gdb will be removed since we already have the env variable
HsailAgentStatus AgentContext::SetLogging(const HsailLogCommand loggingConfig) const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    switch (loggingConfig)
    {
        case HSAIL_LOGGING_ENABLE_ALL:
            AgentSetLogging(true);
            status = AgentLogSetDBELogging(HWDBG_LOG_TYPE_ALL);
            break;

        case HSAIL_LOGGING_DISABLE_ALL:
            AgentSetLogging(false);
            status = AgentLogSetDBELogging(HWDBG_LOG_TYPE_NONE);
            break;

        default:
            AGENT_ERROR("SetLogging: Invalid input");
            status = HSAIL_AGENT_STATUS_FAILURE;
    }

    return status;
}

// The WaitForEvent function returns the type of event type that the DBE received
HsailAgentStatus AgentContext::WaitForEvent(HwDbgEventType* pEventTypeOut)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    if (pEventTypeOut == nullptr)
    {
        AGENT_ERROR("WaitForEvent: pEventTypeOut is nullptr");
        return status;
    }

    HwDbgStatus dbeStatus = HWDBG_STATUS_ERROR;
    dbeStatus = HwDbgWaitForEvent(m_DebugContextHandle,
                               10,
                               pEventTypeOut);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("WaitForEvent: Error in WaitForEvent " <<  GetDBEStatusString(dbeStatus));
        assert(dbeStatus == HWDBG_STATUS_SUCCESS);
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
        m_LastEventType = *pEventTypeOut;
    }

    return status;
}


// A string printer for the AgentContext's state information
// Added for logging convenience
const std::string AgentContext::GetAgentStateString() const
{
    switch (m_AgentState)
    {
        case HSAIL_AGENT_STATE_OPEN:
            return "HSAIL_AGENT_STATE_OPEN";

        case HSAIL_AGENT_STATE_BEGIN_DEBUGGING:
            return "HSAIL_AGENT_STATE_BEGIN_DEBUGGING";

        case HSAIL_AGENT_STATE_END_DEBUGGING:
            return "HSAIL_AGENT_STATE_END_DEBUGGING";

        case HSAIL_AGENT_STATE_CLOSED:
            return "HSAIL_AGENT_STATE_CLOSED";

        case HSAIL_AGENT_STATE_UNKNOWN:
            return "HSAIL_AGENT_STATE_UNKNOWN";

        // This should never happen
        default:
            return "[Unknown HSAIL_AGENT_STATE]";
    }
}

// A shutdown call to clean up resources, set state to closed and clear shared mem buffers
// Also called from destructor if not called explicitly
HsailAgentStatus AgentContext::ShutDown(const bool skipDbeShutDown)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    AGENT_LOG("Shutdown: Start to shutdown the AgentContext, wait for the debug thread");

    status = WaitForDebugThreadCompletion();
    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("ShutDown: Error waiting for the debug thread to complete");
    }

    switch (m_AgentState)
    {
        case HSAIL_AGENT_STATE_OPEN:
        {
            AGENT_LOG("ShutDown: Close the AgentContext after cleanup");
            break;
        }

        case HSAIL_AGENT_STATE_BEGIN_DEBUGGING:
        {
            AGENT_LOG("Shutdown: Agent being closed when Debugging is still active");

            HwDbgStatus dbeStatus = HwDbgEndDebugContext(nullptr);

            if (dbeStatus != HWDBG_STATUS_SUCCESS)
            {
                AGENT_ERROR("HwDbgEndDebugContext: Error " <<  GetDBEStatusString(dbeStatus));
            }

            break;
        }

        case HSAIL_AGENT_STATE_END_DEBUGGING:
        {
            break;
        }

        case HSAIL_AGENT_STATE_CLOSED:
        {
            AGENT_LOG("ShutDown: Attempting to close Agent Context multiple times");
            break;
        }

        default:
            AGENT_ERROR("[Unknown HSAIL_AGENT_STATE] during ShutDown");
    }

    if (m_pKernelBinaries.size() > 1)
    {
        AGENT_LOG("Agent Should not have binaries present now");
    }

    // Exit early if it is already closed
    if (m_AgentState == HSAIL_AGENT_STATE_CLOSED)
    {
        AGENT_LOG("ShutDown: Exit Early since Agent is closed already");

        status = HSAIL_AGENT_STATUS_SUCCESS;
        return status;
    }

    if (!skipDbeShutDown)
    {
        // Now close the DBE
        HwDbgStatus dbeStatus = HwDbgShutDown();

        if (dbeStatus  != HWDBG_STATUS_SUCCESS)
        {
            AGENT_ERROR("HwDbgShutdown failed: DBE Status" << GetDBEStatusString(dbeStatus));
        }
    }
    else
    {
        AGENT_LOG("Skipping the HwDbgShutDown call");
    }

    // Free the shared memory for the binaries
    status = FreeBinarySharedMemBuffer();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not free the Binary Shared memory successfully");
    }

    // Delete all the AgentBinary packages, we may have some left over
    for (size_t i = 0; i < m_pKernelBinaries.size(); i++)
    {
        AgentBinary* pBinary = m_pKernelBinaries[i];

        if (pBinary != nullptr)
        {
            delete pBinary;
        }
    }

    if (m_pBPManager != nullptr)
    {
        delete m_pBPManager;
    }

    if (m_pWavePrinter != nullptr)
    {
        delete m_pWavePrinter;
    }

    if (m_pFocusWaveControl != nullptr)
    {
        delete m_pFocusWaveControl;
    }

    m_AgentState = HSAIL_AGENT_STATE_CLOSED;

    return status;
}

// CompareParentPID will check if the child has become an orphan.
// The common wisdom online says to check for parent PID = 1.
// However, we cant compare the parent pid to the commonly known linux init pid = 1.
//
// This is because different flavors of Linux handle the init pid differently.
// For example Ubuntu has a user mode process called init that becomes the
// parent for all orphaned child processes.
// The user mode init process's PID changes for every session.
//
// For this reason, the below solution of just checking for a change against
// the known parent is better and more complete
bool AgentContext::CompareParentPID() const
{
    bool retCode = false;

    // If there has been no change in the parent's pid since the AgentContext's
    // initialization, we can be safe that the parent is still there and the
    // parent is gdb
    if (m_ParentPID == getppid())
    {
        retCode = true;
    }
    else
    {
        AGENT_ERROR("IsParentRunning: Parent of the HSA application has changed or hsail-gdb may have crashed");
    }

    return retCode;
}

// We can check that the destructor should not be called before we end debugging
// We could add a lot of these checks in a Close() function
AgentContext::~AgentContext()
{
    if (m_AgentState != HSAIL_AGENT_STATE_CLOSED)
    {
        // We skip the DBE shutdown since we dont know what state the HSA tools RT is in.
        // We just clean up the AgentContext for now
        HsailAgentStatus status = ShutDown(true);

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("~AgentContext: Context was not shutdown safely");
        }
    }
}

} // End Namespace HwDbgAgent
