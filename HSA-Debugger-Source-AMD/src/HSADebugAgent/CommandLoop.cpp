//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Debug thread functions
//==============================================================================

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// pthread and error codes
#include <errno.h>
#include <pthread.h>

// AMD DBE
#include "AMDGPUDebug.h"

// Agent Headers
#include "AgentBreakpointManager.h"
#include "AgentContext.h"
#include "AgentFocusWaveControl.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentProcessPacket.h"
#include "AgentUtils.h"
#include "AgentWavePrinter.h"
#include "CommandLoop.h"
#include "CommunicationControl.h"

namespace HwDbgAgent
{

static pthread_t DebugEventHandler ;

/// Check if the fifo is empty.
/// \todo The problem with this function is that there is a side effect
/// of the data actually getting read, a better way may be to poll the fifo.
static bool CheckFifoAtEndDebugging(AgentContext* pActiveContext)
{
    int fd  = GetFifoReadEnd();

    HsailCommandPacket incomingPacket;

    int bytesRead = read(fd, &incomingPacket, sizeof(HsailCommandPacket));

    if (bytesRead <= 0)
    {
        AGENT_LOG("CheckFifoAtEndDebugging: Fifo is empty");
        return true;
    }
    else
    {
        AgentLogPacketInfo(incomingPacket);

        // We shouldnt process this packet, see function declaration
        // for the reason why we added this
        AgentProcessPacket(pActiveContext, incomingPacket);

        // We expect continue debugging packets since you may set a function breakpoint,
        // and then not set a kernel breakpoint, in this case the FIFO should only contain
        // a single continue packet
        if (incomingPacket.m_command == HSAIL_COMMAND_CONTINUE)
        {
            AGENT_LOG("CheckFifoAtEndDebugging: Ignore continue since we are ending debug");
            return true;
        }
        else
        {
            AGENT_LOG("CheckFifoAtEndDebugging: Some other commands were in FIFO");
        }
    }

    return false;
}


void RunFifoCommandLoop(AgentContext* pActiveContext, unsigned int runCount)
{
    for (unsigned int i=0; i < runCount; i++)
    {
        RunFifoCommandLoop(pActiveContext);

        // Sleep for 1ms
        usleep(1000);
    }
}

void RunFifoCommandLoop(AgentContext* pActiveContext)
{
    // Read Fifo descriptor, this should not change once created
    int fd  = GetFifoReadEnd();
    int exitSignal = 0;
    int numPackets = 0;

    do
    {
        HsailCommandPacket incomingPacket;

        // Read a packet off the fifo
        assert(fd > 0);

        if (fd <= 0)
        {
            AGENT_ERROR("Invalid FIFO Descriptor");
            break;
        }

        int bytesRead = read(fd, &incomingPacket, sizeof(HsailCommandPacket));

        if (bytesRead <= 0)
        {
            //Nothing to read on fifo, exit this loop now
            exitSignal = 1;
        }
        else
        {
            AgentLogPacketInfo(incomingPacket);
            AgentProcessPacket(pActiveContext, incomingPacket);
            ++numPackets;
        }
    }
    while (exitSignal == 0);

    if (numPackets != 0)
    {
        AGENT_LOG("RunFifoCommandLoop: Exit ReadFIFO Loop..." <<
                  "Read " << numPackets << " packets");
    }
}

/// A function used in the command loop, makes error reporting below more readable
/// We could add a pthread_exit to possibly exit the thread on error but that would
/// cause problems since we'd have multiple exit points
static void CommandLoopStatusCheck(HsailAgentStatus status, const char* msg)
{
    if (status !=  HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Error in CommandLoop");
        AGENT_ERROR(msg);
    }
}

/// Used in the signal interception code to check that the debug thread has completed
HsailAgentStatus WaitForDebugThreadCompletion()
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    AGENT_LOG("WaitForDebugThreadCompletion: Start waiting for debug thread completion");

    int pthreadStatus =  pthread_join(DebugEventHandler, nullptr);

    AGENT_LOG("WaitForDebugThreadCompletion: Finished waiting for debug thread completion");

    // ESRCH is reasonable since we can call BeginDebugging
    // and then call enddebugging, without starting the debug thread
    // This case could come up if we went pass a HSAIL dispatch without setting or hitting
    // the function breakpoint.
    //
    // ESRCH can also come up when we are in predispatchcallback for the first time
    // and haven't yet started the debug thread
    if (pthreadStatus == 0 || pthreadStatus == ESRCH)
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        AGENT_ERROR("WaitForDebugThreadCompletion: pthread_join error: " << pthreadStatus);

    }

    AGENT_LOG("WaitForDebugThreadCompletion: pthread_join returned: " << pthreadStatus);

    return status;
}

static HsailAgentStatus PostBreakpointEventUpdates(AgentContext*    pActiveContext,
                                                   HwDbgEventType   dbeEventType,
                                                   bool*            pIsStopNeeded)
{
    // We define status variable to check return code for all Agent* functions
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // Get the bp manager for this context and update statistics
    if (dbeEventType != HWDBG_EVENT_POST_BREAKPOINT)
    {
        AGENT_ERROR("PostBreakpointEventUpdates: should only be called for post breakpoint stages");
        return status;
    }

    AGENT_LOG("PostBreakpointEventUpdates: Enter PostBreakpointEventUpdates");

    AgentBreakpointManager* bpManager = pActiveContext->GetBpManager();

    if (bpManager == nullptr)
    {
        AGENT_ERROR("Error: Agent's BP manager is nullptr");
        return status;
    }

    AgentWavePrinter* pWavePrinter = pActiveContext->GetWavePrinter();

    if (pWavePrinter == nullptr)
    {
        AGENT_ERROR("Error: Agent's wave printer is nullptr");
        return status;
    }

    AgentFocusWaveControl* pFocusControl = pActiveContext->GetFocusWaveControl();

    if (pFocusControl == nullptr)
    {
        AGENT_ERROR("Error: Focus control is nullptr");
        return status;
    }

    // We just choose a focus wave based on the active waves
    status = pFocusControl->SetFocusWave(pActiveContext->GetActiveHwDebugContext(), nullptr, nullptr);
    CommandLoopStatusCheck(status, "Error: SetFocus");


    // If we want the reason to be nicely printed
    status = bpManager->PrintStoppedReason(dbeEventType,
                                           pActiveContext->GetActiveHwDebugContext(),
                                           pFocusControl,
                                           pIsStopNeeded);
    CommandLoopStatusCheck(status, "Error: PrintStoppedReason");

    // Let gdb know what the dbe told us
    // Just save it to the shmem for now, the bp manager will let gdb know # of waves
    status = pWavePrinter->SendActiveWavesToGdb(dbeEventType,
                                                pActiveContext->GetActiveHwDebugContext());
    CommandLoopStatusCheck(status, "Error: SendActiveWavesToGdb");

    // Update hit counts for all breakpoints
    // Also send notification to gdb for # of waves in buffer
    status = bpManager->UpdateBreakpointStatistics(dbeEventType,
                                                   pActiveContext->GetActiveHwDebugContext());
    CommandLoopStatusCheck(status, "Error: UpdateBreakpointStatistics");

    // Clear all momentary breakpoints:
    status = bpManager->ClearMomentaryBreakpoints(pActiveContext->GetActiveHwDebugContext());
    CommandLoopStatusCheck(status, "Error: ClearMomentaryBreakpoints");

    AGENT_LOG("PostBreakpointEventUpdates: Exit PostBreakpointEventUpdates");

    return status;
}


// This is a guard function to check that we don't get stuck in a infinite loop
// when the DBE returns a timeout multiple times
static int CheckTimeoutCount()
{
    static int timeoutCount = 0;
    static const int maxTimeoutCount = 100;

    timeoutCount++;
    int exitSignal = 0;

    if (timeoutCount == maxTimeoutCount)
    {
        exitSignal = 1;

        // reset the timeout for the next trip
        timeoutCount = 0;
        AGENT_LOG("CheckTimeoutCount: DBE returned TIMEOUT multiple times, exit debug thread");
    }

    return exitSignal;
}

// This function is similar to CheckTimeout count with the additional role
// of checking the parent status by comparing parent PIDs
static HsailParentStatus CheckParentStatus(const AgentContext* pActiveContext)
{
    HsailParentStatus parentStatus;

    static const int maxCheckCount = 10000;
    static int checkCount = 0;

    checkCount ++;

    if (checkCount < maxCheckCount)
    {
        if (pActiveContext->CompareParentPID())
        {
            parentStatus = HSAIL_PARENT_STATUS_GOOD;
        }
        else
        {
            parentStatus = HSAIL_PARENT_STATUS_TERMINATED;
        }
    }
    else
    {
        parentStatus = HSAIL_PARENT_STATUS_CHECK_COUNT_MAX;
        checkCount = 0;
    }

    return parentStatus;
}

/// The DebugEvent loop is run as a separate thread.
/// It is launched from the predispatch callback.
/// The predispatch callback will have created a breakpoint, so we can
/// guarantee that the first pActiveContext->WaitForEvent call will hit
void* DebugEventThread(void* pArgs)
{

    if (pArgs == nullptr)
    {
        AGENT_ERROR("DebugEventThread: pArgs is nullptr");

        // Exit the pthread
        pthread_exit(nullptr);
    }

    // Get the parameter structure and then  dig out the active context from the threadParams
    DebugEventThreadParams* pthreadParams = reinterpret_cast<DebugEventThreadParams*>(pArgs);
    AgentContext* pActiveContext = pthreadParams->m_pHsailAgentContext;

    AGENT_LOG("Start debug event thread: Arguments: " << pthreadParams << "\t"
              << " AgentContext:  " << pthreadParams->m_pHsailAgentContext);

    if (pActiveContext == nullptr)
    {
        AGENT_ERROR("DebugEventThread: pActivecontext is nullptr");

        // Exit the pthread
        pthread_exit(nullptr);

    }

    if (pActiveContext->HasHwDebugStarted() == false)
    {
        AGENT_ERROR("DebugEventThread: Cannot enter DebugEventThread without BeginDebugging");

        // Exit the pthread
        pthread_exit(nullptr);

    }

    // Check the parameters before we enter the loop
    AgentWavePrinter* pWavePrinter = pActiveContext->GetWavePrinter();
    AgentBreakpointManager* pBPManager = pActiveContext->GetBpManager();

    if (pWavePrinter == nullptr || pBPManager == nullptr)
    {
        AGENT_ERROR("DebugEventThread: pWavePrinter or pBPManager is nullptr");

        // Exit the pthread since something is messed up.
        pthread_exit(nullptr);
    }

    AgentNotifyDebugThreadID();

    bool isNormalExit = false;
    int exitSignal = 0;

    AGENT_LOG("Ready to continue = FALSE, since thread will now wait on HwDbgWaitForEvent");
    pActiveContext->m_ReadyToContinue = false;

    do
    {
        HwDbgEventType dbeEventType = HWDBG_EVENT_INVALID;

        // A blocking wait
        HsailAgentStatus waitStatus = pActiveContext->WaitForEvent(&dbeEventType);

        if (waitStatus == HSAIL_AGENT_STATUS_FAILURE)
        {
            AGENT_ERROR("DebugEventThread: pActiveContext->WaitForEvent");
            exitSignal = 1;
        }

        // We define status variable to check return code for all Agent* functions
        HsailAgentStatus status;

        if (dbeEventType == HWDBG_EVENT_POST_BREAKPOINT &&
            waitStatus == HSAIL_AGENT_STATUS_SUCCESS)
        {
            // Do all the necessary updates for post breakpoint
            // Check if we want to stop
            bool isStopNeeded = false;
            status = PostBreakpointEventUpdates(pActiveContext, dbeEventType, &isStopNeeded);
            CommandLoopStatusCheck(status, "Error: Post Breakpoint event updates");

            if (isStopNeeded)
            {
                if (kill(getpid(), SIGTRAP) == -1)
                {
                    // Get out of the thread,
                    // I am not sure on how to handle signaling failure
                    AGENT_ERROR("Could not signal gdb");
                    exitSignal = 1;
                }
                else
                {
                    AGENT_LOG("DebugEventThread: Raise SIGUSR2 to stop");
                }
            }
            else
            {
                // We can continue the dispatch, we didn't find anything worth stopping for
                AGENT_LOG("Continue dispatch without stopping application");
                pActiveContext->m_ReadyToContinue = true;
            }
        }

        if (dbeEventType == HWDBG_EVENT_END_DEBUGGING  &&
            waitStatus == HSAIL_AGENT_STATUS_SUCCESS)
        {
            // The FIFO should have been drained in the previous iteration
            // \todo This assumption will not be true when we remove the tmp breakpoint
            // since in that case, we can go straight to END_DEBUGGING
            if (CheckFifoAtEndDebugging(pActiveContext) == false)
            {
                AGENT_ERROR("Fifo should be empty if we are going to end debugging");
            }

            AGENT_LOG("Call HwDbgEndDebugging from HwDbgWaitForEvent");

            status = pActiveContext->EndDebugging();
            CommandLoopStatusCheck(status, "Error: EndDebugging");

            // Close down things
            exitSignal = 1;
            isNormalExit = true;
            break;
        }

        // The DBE should never return this
        if (dbeEventType == HWDBG_EVENT_INVALID)
        {
            AGENT_ERROR("pActiveContext->WaitForEvent returned HWDBG_EVENT_INVALID");
            exitSignal = 1;
            isNormalExit = false;
            break;
        }

        // In the timeout case, we do more iterations of the WaitForEvent
        //
        // A really long kernel may not signal the debug event, we will then keep reading
        // the FIFO till we get a timeout
        if (dbeEventType == HWDBG_EVENT_TIMEOUT)
        {
            AGENT_LOG("Command Loop timeout");

            // We can get into an infinite loop if the DBE keeps returning a timeout
            exitSignal = CheckTimeoutCount();

            // If we get a timeout, we exit from the debug thread
            if (exitSignal == 1)
            {
                isNormalExit = false;
            }
        }

        AGENT_LOG("Spin till we get a Continue Packet from FIFO, " <<
                  "Context Ready to Continue bit = " << pActiveContext->m_ReadyToContinue);

        // We spin below to ensure that the "continue" packet has come through.
        // Till the continue packet comes through, we are doing something else
        // like expression evaluation or stepping on the host side.
        //
        // Just because a packet has been seen sent by gdb, doesn't mean that
        // the agent will see it instantly and thats why we spin reading the FIFO
        //
        // As part of this interaction we also check the parent's status continuously.
        // If we get a timeout, that means the check has been done way too many times
        // The only reason why this limit is there is because
        // we don't want this to become an infinite loop if something goes wrong
        // on the gdb side
        //
        // We can remove the limit if we could somehow guarantee continue will be sent
        //
        // Note: even if the debug thread is not in focus or we are stepping on the
        // host side, the continue packet will be sent by continue_command() in gdb
        HsailParentStatus parentStatus = HSAIL_PARENT_STATUS_UNKNOWN;

        do
        {
            RunFifoCommandLoop(pActiveContext);
            parentStatus = CheckParentStatus(pActiveContext);

        }
        while ((pActiveContext->m_ReadyToContinue == false) &&
               (parentStatus == HSAIL_PARENT_STATUS_GOOD));

        if (parentStatus == HSAIL_PARENT_STATUS_CHECK_COUNT_MAX)
        {
            AGENT_LOG("Debug thread read the FIFO till the max count\t" <<
                      "AgentContext state: " <<
                      pActiveContext->GetAgentStateString() << "\t"
                      "DBE event: " << GetDBEEventString(dbeEventType));

            if (pActiveContext->m_ReadyToContinue == false)
            {

                AGENT_LOG("FIFO did not contain a ContinuePacket, forcibly try to continue the dispatch");
                // Overwriting the ReadyToContinue is a fallback in desperation to try and continue again
                // If we get the DBE timeout multiple times, the CheckTimeoutCount()
                // will get us out of the debug thread
                pActiveContext->m_ReadyToContinue = true;
            }

        }
        else if (parentStatus == HSAIL_PARENT_STATUS_TERMINATED)
        {
            AGENT_LOG("The parent process has terminated");
            exitSignal = 1;
            isNormalExit = false;
        }
        else if (parentStatus == HSAIL_PARENT_STATUS_GOOD)
        {
            AGENT_LOG("HSAIL parent status is good");
        }

        // Resume the dispatch.
        // We can now be pretty sure that the continue packet has been sent to the agent
        // and we have set the ReadyForContinue.
        // If we reach the case of timeout in reading the FIFO, we just try to resume the
        // waves and wait again
        //
        // m_ReadyToContinue is necessary since calling variable printing multiple times
        // in GDB may resume the dispatch prematurely
        if (pActiveContext->m_ReadyToContinue == true &&
            waitStatus == HSAIL_AGENT_STATUS_SUCCESS)
        {
            status = pActiveContext->ContinueDebugging();
            CommandLoopStatusCheck(status, "Error: ContinueDebugging");

            AGENT_LOG("Call ContinueDebugging from HwDbgWaitForEvent");
        }

        // We cannot continue debugging until we get a post breakpoint event
        pActiveContext->m_ReadyToContinue = false;
    }
    while (exitSignal == 0);

    if (exitSignal != 1)
    {
        AGENT_ERROR("Abnormal termination of debug thread");
    }

    HsailAgentStatus status;
    if (!isNormalExit)
    {
        // We could get a timeout if we missed a debug event or the kernel has
        // already completed, it may not be the end of the world but we should
        // record it and call EndDebugging to cleanup
        AGENT_LOG("Command Loop did not exit normally, will try to end debug");
        status = pActiveContext->ForceCompleteDispatch();
        CommandLoopStatusCheck(status, "Error: ForceCompleteDispatch call at cleanup");
    }
    else
    {
        status = pBPManager->DisableAllBreakpoints(pActiveContext->GetActiveHwDebugContext());
        CommandLoopStatusCheck(status, "Error: DisableAllBreakpoints call at cleanup");

    }

    // Verify that the exitSignal was given
    assert(exitSignal == 1);

    // We can now delete the threadParams structure since it was allocated in the
    // predispatch callback
    delete pthreadParams;

    AGENT_LOG("DebugEventThread: Exit debug event thread");

    // Exit the pthread
    // We dont need to use  in the end Since it is called implicitly by any
    // thread that is not the main thread
    // Seems to cause a SIGABRT
    //pthread_exit(nullptr);
    return nullptr;

}

// This function creates the debug thread that will wait for the debug event from the DBE
// The function is called from the predispatch callback.
// \params Input arguments
HsailAgentStatus CreateDebugEventThread(DebugEventThreadParams* pArgs)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (pArgs == nullptr)
    {
        AGENT_ERROR("CreateDebugEventThread: pArgs cannot be nullptr");
    }
    else
    {
        int retCode ;
        retCode = pthread_create(&DebugEventHandler, nullptr, DebugEventThread, (void*) pArgs);

        if (retCode != 0)
        {
            AGENT_ERROR("Could not create DebugEventThread");
        }
        else
        {
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    return status;
}
}
