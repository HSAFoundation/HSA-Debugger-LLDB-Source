//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Initial functionality to notify gdb about events that the DBE / agent may see
//==============================================================================
#include <cassert>
#include <cstddef>
#include <cstring>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "CommunicationControl.h"

static const std::string AgentGetGDBNotificationString(const HsailNotification notification)
{
    switch (notification)
    {
        case HSAIL_NOTIFY_UNKNOWN:
            return "HSAIL_NOTIFY_UNKNOWN";

        case HSAIL_NOTIFY_BREAKPOINT_HIT:
            return "HSAIL_NOTIFY_BREAKPOINT_HIT";

        case HSAIL_NOTIFY_NEW_BINARY:
            return "HSAIL_NOTIFY_NEW_BINARY";

        case HSAIL_NOTIFY_AGENT_UNLOAD:
            return "HSAIL_NOTIFY_AGENT_UNLOAD";

        case HSAIL_NOTIFY_BEGIN_DEBUGGING:
            return "HSAIL_NOTIFY_BEGIN_DEBUGGING";

        case HSAIL_NOTIFY_END_DEBUGGING:
            return "HSAIL_NOTIFY_END_DEBUGGING";

        case HSAIL_NOTIFY_FOCUS_CHANGE:
            return "HSAIL_NOTIFY_FOCUS_CHANGE";

        case HSAIL_NOTIFY_START_DEBUG_THREAD:
            return "HSAIL_NOTIFY_START_DEBUG_THREAD";

        case HSAIL_NOTIFY_PREDISPATCH_STATE:
            return "HSAIL_NOTIFY_PREDISPATCH_STATE";

        case HSAIL_NOTIFY_AGENT_ERROR:
            return "HSAIL_NOTIFY_AGENT_ERROR";

        // Should never happen
        default:
            return "[UNKNOWN_NOTIFICATION_TYPE]";
    }
}

/// Push the notification on the FIFO
static HsailAgentStatus PushGDBNotification(const HsailNotificationPayload& payload)
{
    int fd = GetFifoWriteEnd();

    int bytesWritten = 0;

    bytesWritten = write(fd, &payload, sizeof(HsailNotificationPayload));

    if (bytesWritten != sizeof(HsailNotificationPayload))
    {
        return HSAIL_AGENT_STATUS_FAILURE;
    }
    else
    {
        AGENT_LOG("Pushed Notification of Type: " <<
                  AgentGetGDBNotificationString(payload.m_Notification));

        return HSAIL_AGENT_STATUS_SUCCESS;
    }

}

// This is needed to push the event loop along in gdb so we move out of linux_nat_wait
// Some uses of this function:
// 1) In some cases, the inferior application seems to complete in the interval
// between stage1 and stage2 of the initialization
// 3) The application could also complete between the time that the new binary is
// received and the resolved breakpoint is sent to the Agent.
//
// This extra kill provides an event to waitpid() in gdb.
// It makes the application get out of linux_nat_wait and then handle other events
// (which may be events on the HSAIL FIFO)
void AgentTriggerGDBEventLoop()
{
    AGENT_LOG("AgentTriggerGDBEventLoop: Push the GDB Linux event loop");

    if (kill(getpid(), SIGUSR1) == -1)
    {
        AGENT_ERROR("Could not raise the SIGUSR");
        // I am not sure on how to handle signaling failure
        return;
    }

}

// Notify the parent that the Agent has loaded
void AgentNotifyGDB()
{
    if (kill(getppid(), SIGALRM) == -1)
    {
        AGENT_ERROR("Could not raise the SIGALRM to GDB");
    }

}

// The following wall of comments explains the notification logic

// The older method for notifying a binary was that we signal gdb that a
// new binary is seen by the agent
// A handler is installed for SIGALRM in GDB's linux native config
// This is an initial and faster solution for notifying GDB that a
// new binary has been detected by the agent
// We need to notify the "parent", in the present case its GDB.
// We are using the getppid system call so that we get gdb's pid
// We cannot use pthread_kill here since we are signalling an external process
// if (kill(getppid(), SIGALRM) == -1)
// {
//    AgentErrorLog("Error in signalling");
//    return status;
// }


// The above manner had two flaws
// 1)   The signal handler may have to do real work, which is not good since
//      the handler should be small and re-entrant
// 2)   For this reason, there is no graceful manner to go from the handler to any of the
//      hsail-specific functionality that could be started within gdb
//      on the receipt of this signal.

// An alternative way to do this signalling is that we use
// GDB's self pipe based event processing queue, then we dont need the "kill" in the AgentNotifyNewBinary
// In the self-pipe, we add the read end of the fifo as one of the file descriptors polled by GDB
//
// When something new is added to the fifo, a callback function will be run by gdb
// The callback would read the packet and call the right portion of gdb / hsail facilities
// To use self-pipe only works gdb 7.8 since the target-async mode
// which uses a self-pipe is on by default for Linux in 7.8



// Notify the parent that a new binary has been added
// GDB will use the binary to set up debug facilities
// \todo Add the hl symbol and the ll symbol
HsailAgentStatus AgentNotifyNewBinary(const size_t binarySize,
                                      const std::string& hlSymbolName, const std::string& llSymbolName,
                                      const std::string& kernelName, const HsailWaveDim3& workGroupSize,
                                      const HsailWaveDim3& gridSize)
{

    // Initialize the payload and zero it out
    HsailNotificationPayload newBinaryPayload;
    memset(&newBinaryPayload, 0, sizeof(HsailNotificationPayload));

    newBinaryPayload.m_Notification = HSAIL_NOTIFY_NEW_BINARY;
    newBinaryPayload.payload.BinaryNotification.m_binarySize = reinterpret_cast<uint64_t>(binarySize);

    memcpy(newBinaryPayload.payload.BinaryNotification.m_hlSymbolName,
           hlSymbolName.c_str(),
           hlSymbolName.size() + 1);

    memcpy(newBinaryPayload.payload.BinaryNotification.m_llSymbolName,
           llSymbolName.c_str(),
           llSymbolName.size() + 1);

    memcpy(newBinaryPayload.payload.BinaryNotification.m_KernelName,
           kernelName.c_str(),
           kernelName.size() + 1);

    newBinaryPayload.payload.BinaryNotification.m_workGroupSize.x = workGroupSize.x;
    newBinaryPayload.payload.BinaryNotification.m_workGroupSize.y = workGroupSize.y;
    newBinaryPayload.payload.BinaryNotification.m_workGroupSize.z = workGroupSize.z;

    newBinaryPayload.payload.BinaryNotification.m_gridSize.x = gridSize.x;
    newBinaryPayload.payload.BinaryNotification.m_gridSize.y = gridSize.y;
    newBinaryPayload.payload.BinaryNotification.m_gridSize.z = gridSize.z;

    HsailAgentStatus status =  PushGDBNotification(newBinaryPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AgentErrorLog("Error in Pushing a new binary notification to GDB\n");
        return status;
    }

    return status;
}

// Send the notification packet that we have hit a breakpoint, includes information about
// the size of the wave info buffer
HsailAgentStatus AgentNotifyBreakpointHit(const HsailNotificationPayload numWavesPayload)
{
    // Just check the payload type and send it off
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (numWavesPayload.m_Notification != HSAIL_NOTIFY_BREAKPOINT_HIT)
    {
        AgentErrorLog("Error in Pushing a breakpoint hit notification to GDB\n");
        return status;
    }

    status =  PushGDBNotification(numWavesPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AgentErrorLog("Error in Pushing a breakpoint hit notification to GDB\n");
        return status;
    }

    return status;

}

// Tell GDB when the focus thread changes
// The focus wave can be changed by the old focus wave completing or an explicit user command
HsailAgentStatus AgentNotifyFocusChange(const HwDbgDim3& focusWorkGroup,
                                        const HwDbgDim3& focusWorkItem)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    HsailNotificationPayload changeFocus;
    memset(&changeFocus, 0, sizeof(HsailNotificationPayload));

    changeFocus.m_Notification = HSAIL_NOTIFY_FOCUS_CHANGE;

    // Copy the members individually since gdb has HsailWaveDim3 and DBE struct is HwDbgDim3
    changeFocus.payload.FocusChange.m_focusWorkGroup.x = focusWorkGroup.x;
    changeFocus.payload.FocusChange.m_focusWorkGroup.y = focusWorkGroup.y;
    changeFocus.payload.FocusChange.m_focusWorkGroup.z = focusWorkGroup.z;
    changeFocus.payload.FocusChange.m_focusWorkItem.x = focusWorkItem.x;
    changeFocus.payload.FocusChange.m_focusWorkItem.y = focusWorkItem.y;
    changeFocus.payload.FocusChange.m_focusWorkItem.z = focusWorkItem.z;

    status =  PushGDBNotification(changeFocus);

    return status;
}

HsailAgentStatus AgentNotifyKillComplete(const bool isKillSuccess, const bool isQuitCommandIssued)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    HsailNotificationPayload killComplete;
    memset(&killComplete, 0, sizeof(HsailNotificationPayload));
    killComplete.m_Notification = HSAIL_NOTIFY_KILL_COMPLETE;

    // Just pass through arguments
    killComplete.payload.KillCompleteNotification.killSuccessful = isKillSuccess;
    killComplete.payload.KillCompleteNotification.isQuitCommandIssued = isQuitCommandIssued;

    status =  PushGDBNotification(killComplete);

    return status;

}

// Tell gdb when we enter and leave predispatch callbacks
// Used to control single stepping into a GPU dispatch from a function breakpoint
HsailAgentStatus AgentNotifyPredispatchState(const HsailPredispatchState ipState)
{
    HsailNotificationPayload predispatchPayload;
    memset(&predispatchPayload, 0, sizeof(HsailNotificationPayload));

    predispatchPayload.m_Notification = HSAIL_NOTIFY_PREDISPATCH_STATE;
    predispatchPayload.payload.PredispatchNotification.m_predispatchState = ipState;
    HsailAgentStatus status =  PushGDBNotification(predispatchPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AgentErrorLog("Error in Pushing a new Predispatch notification to GDB\n");
        return status;
    }

    return status;
}

// Tell gdb that the dispatch is completed and to end debugging
// This will restore how gdb prints exceptions and signal information back to the original style
HsailAgentStatus AgentNotifyBeginDebugging(const bool setDeviceFocus)
{
    // Initialize the payload and zero it out
    HsailNotificationPayload beginDebugPayload;
    memset(&beginDebugPayload, 0, sizeof(HsailNotificationPayload));

    beginDebugPayload.m_Notification = HSAIL_NOTIFY_BEGIN_DEBUGGING;
    beginDebugPayload.payload.BeginDebugNotification.setDeviceFocus = setDeviceFocus;
    HsailAgentStatus status =  PushGDBNotification(beginDebugPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AGENT_ERROR("Error in Pushing a BeginDebugging notification to GDB\n");
        return status;
    }

    return status;
}


// Tell gdb that the dispatch is completed and to end debugging
// This will restore how gdb prints exceptions and signal information back to the original style
HsailAgentStatus AgentNotifyEndDebugging(const bool hasDispatchCompleted)
{
    // Initialize the payload and zero it out
    HsailNotificationPayload endDebugPayload;
    memset(&endDebugPayload, 0, sizeof(HsailNotificationPayload));

    endDebugPayload.m_Notification = HSAIL_NOTIFY_END_DEBUGGING;
    endDebugPayload.payload.EndDebugNotification.hasDispatchCompleted = hasDispatchCompleted;
    HsailAgentStatus status =  PushGDBNotification(endDebugPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AgentErrorLog("Error in Pushing a new binary notification to GDB\n");
        return status;
    }

    return status;
}

// Tell gdb the debug thread ID so it knows when to single step the CPU or the GPU
HsailAgentStatus AgentNotifyDebugThreadID()
{
    HsailNotificationPayload startDebugThreadPayload;
    memset(&startDebugThreadPayload, 0, sizeof(HsailNotificationPayload));

    int sid = syscall(SYS_gettid);

    startDebugThreadPayload.m_Notification = HSAIL_NOTIFY_START_DEBUG_THREAD;
    startDebugThreadPayload.payload.StartDebugThreadNotification.m_tid = sid;

    HsailAgentStatus status =  PushGDBNotification(startDebugThreadPayload);

    if (HSAIL_AGENT_STATUS_SUCCESS != status)
    {
        AgentErrorLog("Error in Pushing a new debug thread notification to GDB\n");
    }

    return status;
}
