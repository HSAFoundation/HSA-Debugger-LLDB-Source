//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief  Implementation of pre- and post-dispatch callback
//==============================================================================
#include <hsa_ext_amd.h>
#include <amd_hsa_tools_interfaces.h>
#include <sys/wait.h>

#include <pthread.h>

#include "AgentBinary.h"
#include "AgentBreakpointManager.h"
#include "AgentContext.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentProcessPacket.h"
#include "AgentUtils.h"
#include "CommandLoop.h"

namespace HwDbgAgent
{

static void PredispatchCheckStatus(const HsailAgentStatus status, const char* ip)
{
    if (status !=  HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("PredispatchCallback: " << ip);
    }
}

static bool ValidateAQLDimensions(const uint32_t GRID_SIZE, const uint32_t WORK_GROUP_SIZE)
{
    bool ret = true;

    if (GRID_SIZE == 0)
    {
        AGENT_WARNING("AQL grid_size cannot be 0.");
        ret &= false;
    }

    if (WORK_GROUP_SIZE == 0)
    {
        AGENT_WARNING("AQL work_group_size cannot be 0.");
        ret &= false;
    }

    if (GRID_SIZE < WORK_GROUP_SIZE)
    {
        AGENT_WARNING("AQL grid_size " << GRID_SIZE << " shouldn't be less than work_group_size "
                      << WORK_GROUP_SIZE << ".");
        ret &= false;
    }

    return ret;
}

static bool ValidateAQL(const hsa_kernel_dispatch_packet_t& AQL)
{
    // Check grid_size and work_group_size
    // We only give warning to the user if dimensions setup is incorrect.
    // The AQL packet would still be dispatched in this case.
    if (!ValidateAQLDimensions(AQL.grid_size_x, AQL.workgroup_size_x))
    {
        AGENT_WARNING("AQL dimension x setup incorrect.\n");
    }

    if (!ValidateAQLDimensions(AQL.grid_size_y, AQL.workgroup_size_y))
    {
        AGENT_WARNING("AQL dimension y setup incorrect.\n");
    }

    if (!ValidateAQLDimensions(AQL.grid_size_z, AQL.workgroup_size_z))
    {
        AGENT_WARNING("AQL dimension z setup incorrect.\n");
    }

    // TODO: check the rest of AQL field.

    return true;
}

void PreDispatchCallback(const hsa_dispatch_callback_t* pRTParam, void* pUserArgs)
{
    AGENT_LOG("== Start Pre-dispatch callback ==");

    if (pRTParam == nullptr || !pRTParam->pre_dispatch)
    {
        AGENT_ERROR("PreDispatchCallback: Invalid input RT parameters");
        return;
    }

    hsa_kernel_dispatch_packet_t* pAqlPacket = pRTParam->aql_packet;

    if (pAqlPacket == nullptr)
    {
        AGENT_ERROR("No AQL packet present.");
        return;
    }

    if (!ValidateAQL(*pAqlPacket))
    {
        AGENT_ERROR("Invalid AQL packet.");
        return;
    }

    if (pUserArgs == nullptr)
    {
        AGENT_ERROR("AgentContext pointer is not valid");
        return;
    }

    AgentContext* pActiveContext = reinterpret_cast<AgentContext*>(pUserArgs);

    if (pActiveContext == nullptr)
    {
        AGENT_ERROR("Invalid AgentContext from PredispatchCallback");
        return;
    }

    // Wait for the debug thread that has been spawned previously to complete
    HsailAgentStatus status = WaitForDebugThreadCompletion();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not wait for debug thread completion in predispatchcallback, " <<
                    "Subsequent dispatches may not be debugged correctly");
        return;
    }

    AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();

    if (pBpManager == nullptr)
    {
        AGENT_ERROR("Predispatchcallback: Invalid BpManager from pActiveContext\n");

        // We really shouldnt proceed without a valid BpManager
        return;
    }

     AgentLogAQLPacket(pAqlPacket);

    // We should read the fifo command loop and check for any function or source breakpoints
    // We will consume everything in the FIFO but stop in the predispatch only if any kernel
    // function breakpoints are set, 50 is just a heuristic for now.
    RunFifoCommandLoop(pActiveContext, 50);

    // Check if we have any pending function breakpoints
    int pendingFunctionNameBP =
        pBpManager->GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_PENDING,
                                             HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP) +
        pBpManager->GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_ENABLED,
                                             HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP);

    AGENT_LOG("PredispatchCallback: Begin Debugging: " <<
              "# of Pending function breakpoints " << pendingFunctionNameBP << "\t" <<
              "AQL Packet address " << pAqlPacket);

    // We always have to start debugging and send the binary to GDB now
    // We will check if we have any function breakpoints pending and will accordingly stop
    // We pass the parameters from the predispatch callback to the Agent Context
    status = pActiveContext->BeginDebugging(pRTParam->agent,
                                            pRTParam->queue,
                                            pAqlPacket,
                                            HWDBG_BEHAVIOR_DISABLE_DISPATCH_DEBUGGING);
    PredispatchCheckStatus(status, "Error in BeginDebugging");

    if (status != HSAIL_AGENT_STATUS_SUCCESS || pActiveContext->HasHwDebugStarted() == false)
    {
        return;
    }

    // Do all the Binary handling
    AgentBinary* pBinary = nullptr;
    pBinary = new(std::nothrow) AgentBinary;

    if (pBinary == nullptr)
    {
        AGENT_ERROR("PredispatchCallback: Could not allocate a binary, exiting predispatchcallback\n");
        return;
    }

    status = AgentNotifyPredispatchState(HSAIL_PREDISPATCH_ENTERED_PREDISPATCH);
    PredispatchCheckStatus(status, "Error notifying predispatch state!");

    status = pBinary->PopulateBinaryFromDBE(pActiveContext->GetActiveHwDebugContext(), pAqlPacket);
    PredispatchCheckStatus(status, "Error in Populating Binary");

    // Logging, save the binary and the ISA (if enabled)
    AgentLogSaveBinaryToFile(pBinary, pAqlPacket);

    // Set the workgroup size and workgroup dimensions
    pBinary->PopulateWorkgroupSizeInformation(pActiveContext);

    // We need this AgentBinary object and will save it within the agent context
    // The AgentBinary will be deleted when we do EndDebugging
    status = pActiveContext->AddKernelBinaryToContext(pBinary);
    PredispatchCheckStatus(status, "Error saving binary in AgentContext!");

    // We notify gdb since this is a new binary, we still don't know
    // if we will use it for kernel debugging yet though.
    // This is because we dont know if there are any kernel breakpoints set yet
    status = pBinary->NotifyGDB();
    PredispatchCheckStatus(status, "Error in notifying GDB!");

    AGENT_LOG("PredispatchCallback: Check for Function breakpoints");
    // Search for a kernel name match if any function breakpoints present
    bool isFuncBPStopNeeded = false;
    int funcBPPosition = -1;

    if (0 < pendingFunctionNameBP)
    {
        std::string kernelName = pBinary->GetKernelName();

        if (kernelName.empty())
        {
            AGENT_ERROR("KernelName should not be empty\n");
        }

        if (pBpManager->CheckAgainstKernelNameBreakpoints(kernelName, &funcBPPosition))
        {
            assert(funcBPPosition != -1);
            isFuncBPStopNeeded = true;

            // Print the function breakpoint info and send notification to gdb
            status = pBpManager->ReportFunctionBreakpoint(kernelName);
            PredispatchCheckStatus(status, "Error in Reporting function BP");
        }
    }


    if (isFuncBPStopNeeded)
    {
        status = pActiveContext->RaiseTrapSignal();
        PredispatchCheckStatus(status, "Error  Raising the trap signal!");
    }
    else
    {

    }

    // We are going to enter kernel debugging
    // Set all the existing breakpoints again, do this before you run the FIFO
    // since the FIFO may include commands to disable breakpoints
    status = pBpManager->EnableAllPCBreakpoints(pActiveContext->GetActiveHwDebugContext());
    PredispatchCheckStatus(status, "Error in Enabling existing PC Breakpoints");

    // We should check the fifo and set the breakpoints in this thread itself.
    //
    // This is necessary so that the appropriate source breakpoints
    // are ready before the kernel starts
    RunFifoCommandLoop(pActiveContext, 50);

    // We need to check again if any breakpoints were created
    // In case the user set any breakpoints or asked to step
    int numPendingSrcBP =
        pBpManager->GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_ENABLED, HSAIL_BREAKPOINT_TYPE_PC_BP) +
        pBpManager->GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_PENDING, HSAIL_BREAKPOINT_TYPE_PC_BP) +
        pBpManager->GetNumMomentaryBreakpointsInState(HSAIL_BREAKPOINT_STATE_PENDING) +
        pBpManager->GetNumMomentaryBreakpointsInState(HSAIL_BREAKPOINT_STATE_ENABLED);

    status = pBpManager->DisableAllBreakpoints(pActiveContext->GetActiveHwDebugContext());
    PredispatchCheckStatus(status, "Error in DisableAllBreakpoints");

    status = pActiveContext->EndDebugging();
    PredispatchCheckStatus(status, "Error in EndDebugging when no source breakpoints found");

    if (0 >= numPendingSrcBP)
    {
        AGENT_LOG("No source breakpoints available, exiting predispatch callback without starting debug thread");

        status = AgentNotifyPredispatchState(HSAIL_PREDISPATCH_LEFT_PREDISPATCH);
        PredispatchCheckStatus(status, "Error notifying predispatch state after EndDebugging");
        return;
    }
    else
    {
        status = pActiveContext->BeginDebugging(pRTParam->agent,
                                                pRTParam->queue,
                                                pAqlPacket,
                                                HWDBG_BEHAVIOR_NONE);
        PredispatchCheckStatus(status, "Error in Begin Debugging the second time in the predispatch");

        AGENT_LOG("Debug thread will be needed for this dispatch, "
                  << numPendingSrcBP << " source breakpoints enabled");

        status = pBpManager->EnableAllPCBreakpoints(pActiveContext->GetActiveHwDebugContext());
        PredispatchCheckStatus(status, "Error in Enabling existing PC Breakpoints");

    }

    // Start Debug Thread Stage
    // We allocate the DebugEventThreadParams on the heap
    // The memory allocated here is freed in DebugEventThread, once the data is consumed
    DebugEventThreadParams* pDebugThreadArgs = new(std::nothrow) DebugEventThreadParams;

    if (nullptr == pDebugThreadArgs)
    {
        AGENT_ERROR("Could not allocate debugThreadArgs. Did not spawn debug thread");
    }
    else
    {
        // Pass the agent context that was initialized to the Debug thread
        pDebugThreadArgs->m_pHsailAgentContext = reinterpret_cast<AgentContext*>(pActiveContext);

        // Log state of the agent
        AGENT_LOG("PredispatchCallback: \t" <<
                  "AgentContext State: " << pActiveContext->GetAgentStateString() << "\t" <<
                  "Debug Thread args: " << pDebugThreadArgs << "\t" <<
                  "AgentContext is:  " << pDebugThreadArgs->m_pHsailAgentContext);

        SetEvaluatorActiveContext(pActiveContext);

        SetKernelParametersBuffers(pAqlPacket);

        status = CreateDebugEventThread(pDebugThreadArgs);
        PredispatchCheckStatus(status, "Error in CreateDebugEventThread");
    }

    status = AgentNotifyPredispatchState(HSAIL_PREDISPATCH_LEFT_PREDISPATCH);
    PredispatchCheckStatus(status, "Error notifying predispatch state");

    return;
}


// Note: We cannot use HSAWaitOnSignal here to close the debug Event thread
// since the words *PostDispatch* mean just that, they do not mean PostCompletion
void PostDispatchCallback(const hsa_dispatch_callback_t* pRTParam, void* pUserArgs)
{
    AGENT_LOG("== Post-dispatch callback ==");

    if (pRTParam == nullptr || pRTParam->pre_dispatch)
    {
        AGENT_ERROR("PostDispatchCallback: Invalid input RT parameters");
        return;
    }
}

}   // End Namespace HwDbgAgent
