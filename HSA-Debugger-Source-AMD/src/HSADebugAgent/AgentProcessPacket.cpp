//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Agent processing, functions to consume FIFO packets and configure expression evaluation
//==============================================================================
#include <iostream>
#include <cassert>
// \todo including cstdint seems to need a -std=c++11 compiler switch on gcc
// Maye better to avoid C++11 for now.
#include <cstdbool>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <stdio.h>

// Use some stl vectors for maintaining breakpoint handles
#include <algorithm>
#include <vector>

// Agent includes
#include "AgentBreakpointManager.h"
#include "AgentContext.h"
#include "AgentFocusWaveControl.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentProcessPacket.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"

// Add DBE (Version decided by Makefile)
#include "AMDGPUDebug.h"

static void DBEDeleteBreakpoint(HwDbgAgent::AgentContext* pActiveContext,
                                const HsailCommandPacket& ipPacket)
{

    if (!pActiveContext->HasHwDebugStarted())
    {
        AgentErrorLog("DBEDeleteBreakpoint:BeginDebugging has not occured\n");
    }
    else
    {

        HwDbgAgent::AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();

        HsailAgentStatus status;

        status = pBpManager->DeleteBreakpoint(pActiveContext->GetActiveHwDebugContext(),
                                              ipPacket);

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AgentErrorLog("DBEDeleteBreakpoint: Could not delete breakpoint\n");
        }

    }

    return;
}



static void DBECreateBreakpoint(HwDbgAgent::AgentContext* pActiveContext,
                                const HsailCommandPacket& ipPacket)
{

    HwDbgAgent::AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();
    HwDbgAgent::HsailBkptType bpType = HwDbgAgent::HSAIL_BREAKPOINT_TYPE_PC_BP;

    if ((char)0 != ipPacket.m_kernelName[0])
    {
        bpType = HwDbgAgent::HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP;
    }

    HsailAgentStatus status;
    status = pBpManager->CreateBreakpoint(pActiveContext->GetActiveHwDebugContext(),
                                          ipPacket,
                                          bpType);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AgentErrorLog("DBECreateBreakpoint: Could not create a breakpoint\n");
    }

}

static void DBEDisablePCBreakpoint(HwDbgAgent::AgentContext* pActiveContext,
                                   const HsailCommandPacket& ipPacket)
{
    if (!pActiveContext->HasHwDebugStarted())
    {
        AgentErrorLog("DBEDisablePCBreakpoint:should not Disable breakpoint without BeginDebugging\n");
    }
    else
    {
        HwDbgAgent::AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();

        HsailAgentStatus status;
        status = pBpManager->DisablePCBreakpoint(pActiveContext->GetActiveHwDebugContext(),
                                                 ipPacket);

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AgentErrorLog("DBEDisablePCBreakpoint: Could not disable a breakpoint\n");
        }
    }

    return;
}

static void DBEEnablePCBreakpoint(HwDbgAgent::AgentContext* pActiveContext,
                                  const HsailCommandPacket& ipPacket)
{
    if (!pActiveContext->HasHwDebugStarted())
    {
        AgentErrorLog("DBE: We should not Enable breakpoint without BeginDebugging \n");
    }
    else
    {
        HwDbgAgent::AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();

        HsailAgentStatus status;
        status = pBpManager->EnablePCBreakpoint(pActiveContext->GetActiveHwDebugContext(),
                                                ipPacket);

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AgentErrorLog("DBEEnablePCBreakpoint: Could not enable a breakpoint\n");
        }
    }

    return;
}

static void DBEMomentaryBreakpoint(HwDbgAgent::AgentContext* pActiveContext,
                                   const HsailCommandPacket& ipPacket)
{
    HwDbgAgent::AgentBreakpointManager* pBpManager = pActiveContext->GetBpManager();

    HsailAgentStatus status;
    status = pBpManager->CreateMomentaryBreakpoints(pActiveContext->GetActiveHwDebugContext(),
                                                    ipPacket);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AgentErrorLog("DBEMomentaryBreakpoint: Could not create a momentary breakpoint\n");
    }
}

// Global pointer to active context used for the expression evaluator
// Can be fixed soon by checking a static variable in the function
HwDbgAgent::AgentContext* g_ActiveContext = nullptr;

void SetEvaluatorActiveContext(HwDbgAgent::AgentContext* activeContext)
{
    g_ActiveContext = activeContext;
}

// Global pointer to kernel parameters buffers used for the var eval
// isaMemoryRegion type
void* g_KernelParametersBuffer = nullptr;

void SetKernelParametersBuffers(const hsa_kernel_dispatch_packet_t* pAqlPacket)
{
    if (nullptr == pAqlPacket)
    {
        g_KernelParametersBuffer = nullptr;
    }
    else
    {
        g_KernelParametersBuffer = (void*)pAqlPacket->kernarg_address;
    }
}

void KillHsailDebug(bool isQuitIssued)
{
    AGENT_LOG("KillHsailDebug: isQuitIssued: " << isQuitIssued);

    HsailAgentStatus status = g_ActiveContext->KillDispatch(isQuitIssued);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("KillDispatch: Killing the dispatch by expression evaluation");
    }

    status = g_ActiveContext->EndDebugging();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("KillDispatch: Ending debugging from within expression evaluation");
    }

}

bool GetPrivateMemory(HwDbgDim3 workGroup, HwDbgDim3 workItem, size_t base, size_t offset, size_t numByteToRead, void* pMemOut, size_t* pNumBytesOut)
{
    bool retVal = false;

    HwDbgDim3 workGroupId;
    workGroupId.x = workGroup.x;
    workGroupId.y = workGroup.y;
    workGroupId.z = workGroup.z;

    HwDbgDim3 workItemId;
    workItemId.x = workItem.x;
    workItemId.y = workItem.y;
    workItemId.z = workItem.z;

    HwDbgStatus status = HWDBG_STATUS_SUCCESS;

    if (g_ActiveContext == nullptr)
    {
        AgentErrorLog("GetPrivateMemory: Active context is nullptr\n");
        (*(int*)pMemOut) = 0;
        return false;
    }


    /*    char buffer[256];
        sprintf(buffer,"GPM WG:%d,%d,%d WI:%d,%d,%d realLocation:%ld\n", workGroup.x, workGroup.y, workGroup.z, workItem.x, workItem.y, workItem.z, (size_t)pMemOut);
        AgentLog(buffer);

        sprintf(buffer,"GPM AC:%x, DX:%x, base:%ld, base+offset:%ld, bytes to read:%ld\n", g_ActiveContext, (void*)g_ActiveContext->GetActiveHwDebugContext(), base, base+offset, numByteToRead);
        AgentLog(buffer);

        sprintf(buffer,"Address:%x\n", pMemOut);
        AgentLog(buffer);*/

    status = HwDbgReadMemory(g_ActiveContext->GetActiveHwDebugContext(),
                             1 /* IMR_Scratch */,
                             workGroupId, workItemId,
                             base + offset,
                             numByteToRead,
                             pMemOut,
                             pNumBytesOut);

    if (status != HWDBG_STATUS_SUCCESS)
    {
        AgentErrorLog("GetPrivateMemory: Error in Printing\n");
        AgentErrorLog(GetDBEStatusString(status).c_str());
    }
    else
    {
        retVal = true;
    }

    return retVal;
}

enum LocationRegister
{
    LOC_REG_REGISTER,   ///< A register holds the value
    LOC_REG_STACK,      ///< The frame pointer holds the value
    LOC_REG_NONE,       ///< No registers are to be used in getting the value
    LOC_REG_UNINIT,     ///< Default / max value
};

// This function is called by the expression evaluator
void SetHsailThreadCmdInfo(unsigned int flatWg,
                            unsigned int wiX, unsigned int wiY, unsigned int wiZ)
 {
     char buffer[256];
    
    unsigned sx = g_ActiveContext->m_workGroupSize.x;
    unsigned sy = g_ActiveContext->m_workGroupSize.y;
    unsigned x,y,z;

    x = flatWg % sx;
    y = (flatWg - x) % (sx*sy) / sx;
    z = (flatWg - x - (y * sx)) / sx * sy;

    HwDbgDim3 focusWg;
    focusWg.x = x;
    focusWg.y = y;
    focusWg.z = z;

    HwDbgDim3 focusWI;
    focusWI.x = wiX;
    focusWI.y = wiY;
    focusWI.z = wiZ;

    HsailAgentStatus status = g_ActiveContext->m_pFocusWaveControl->SetFocusWave(nullptr, &focusWg, &focusWI);

    sprintf(buffer, "SetHsailThreadCmdInfo: got here wg:%d %d %d, wi:%d %d %d \n",
            focusWg.x, focusWg.y, focusWg.z,
            focusWI.x, focusWI.y, focusWI.z);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AgentErrorLog("Could not change focus wave from GDB command\n");
        AgentErrorLog(buffer);
    }

    AgentLog(buffer);
}

void* gVariableValueForRelease = nullptr;
void FreeVarValue(void)
{
    free(gVariableValueForRelease);
    gVariableValueForRelease = nullptr;
}

void* GetVarValue(int reg_type, size_t var_size, unsigned int reg_num, bool deref_value, unsigned int offset, unsigned int resource, unsigned int isa_memory_region, unsigned int piece_offset, unsigned int piece_size, int const_add)
{
    // Results
    /* prepare the largest primitive buffer but the GetPrivateMemory will get the var_size */
    void* variableValues = malloc(8);
    memset(variableValues, 0, 8);

    // unsigned int valueStride = 0;

    // 1. Get the base location:
    static const size_t zero = 0;
    const void* loc = nullptr;
    //  size_t locStride = 0;

    /* offset for step 2 */
    size_t totalOffset = 0;

    /* gdb later turns this to nullptr so we need a copy for releasing this */
    gVariableValueForRelease = variableValues;

    if (nullptr == variableValues)
    {
        return nullptr;
    }

    switch (reg_type)
    {
        case LOC_REG_REGISTER:
        {
        }
        break;

        case LOC_REG_STACK:
        {
        }
        break;

        case LOC_REG_NONE:
        {
            loc = &zero;
        }
        break;

        case LOC_REG_UNINIT:
        {
            // This is currently the information for some unsupported locations, (e.g. __local T* parameters):
            return nullptr;
        }
        break;

        default:
        {
            AGENT_LOG("hsail-printf unsupported reg type");
        }
        break;
    }

    /* 2. Dereference and apply offset as needed: */
    /* currently ignoring array offset */
    totalOffset = offset;

    if (deref_value)
    {
        // Note: This assumes for dereferenced types that the values of the base pointer are all the same.
        // A more "correct" method would be to iterate all the active work items, get the value for each,
        // then copy that into a buffer.

        size_t realLocation = *((size_t*)loc) + totalOffset + piece_offset;

        // Since we applied the piece offset here (to get the correct value), we can reset the piece offset we will use to parse to 0:
        piece_offset = 0;

        switch (isa_memory_region)
        {

            case 0: // = IMR_Global
            {
                // Global Memory:
                memcpy(variableValues, ((void*)realLocation), var_size);
                // valueStride = (unsigned int)var_size;
            }
            break;

            case 1: // = IMR_Scratch
            {
                // Private memory:
                size_t locVarSize;
                /* for some reason only in the first time allocation if I do not erase the variableValues than the data that is received from
                   GetPrivateMemory is faulty. freeing and allocating the memory solves this problem. This a temporary solution */
                HwDbgDim3 focusWg;
                HwDbgDim3 focusWi;
                HsailAgentStatus status = g_ActiveContext->m_pFocusWaveControl->GetFocus(focusWg, focusWi);

                if (status != HSAIL_AGENT_STATUS_SUCCESS)
                {
                    AGENT_ERROR("Could not get focus parameters");
                }

                bool rc = GetPrivateMemory(focusWg, focusWi,
                                           (size_t)realLocation, 0, var_size, variableValues, &locVarSize);

                if (rc)
                {
                    // Only signify success if we actually asked for and got a value from the DBE:
                    /* valueStride = (unsigned int)locVarSize; return value is used in original ref function */
                }
            }
            break;

            case 2: // = IMR_Group
            {
                // Local memory:
                // Not currently supported
                return nullptr;
            }
            break;

            case 3: // = IMR_ExtUserData
            {
                // Uri, 21/05/13 - As a workaround to getting an ExtUserData (32-bit aligned OpenCL arguments buffer) pointer
                // And having to read from the AQL (64-bit aligned HSA arguments buffer), we need to double the offset here,
                // As this works properly for most parameters.
                // Should be revised (as parameters larger than 32-bit will not work) or removed when the compiler moves to AQL offsets.
                realLocation *= 2;
            }

            case 4: // = IMR_AQL
            case 5: // = IMR_FuncArg
            {
                // assume kernel argument is not nullptr but value is 0?
                // Kernel arguments:
                // Add the offset to the kernel args base pointer:
                realLocation += (size_t)g_KernelParametersBuffer;
                memcpy(variableValues, ((void*)realLocation), var_size);
                /* valueStride = (unsigned int)var_size; return value is used in original ref function */

            }
            break;
        }
    }
    else
    {
        /* valueStride = 0;  return value is used in original ref function */
        variableValues = (void*)((size_t)loc + totalOffset);
    }

    variableValues = (void*)((size_t)variableValues + piece_offset);

    return (void*)(*(size_t*)variableValues);
}

void AgentProcessPacket(HwDbgAgent::AgentContext* pActiveContext,
                        const HsailCommandPacket& packet)
{
    switch (packet.m_command)
    {
        // \todo What is the earliest time that this packet can be sent  ?
        // Is it theoretically possible for GDB to send this packet ?
        // This packet is not needed since we do the setup in the predispatch
        case HSAIL_COMMAND_BEGIN_DEBUGGING:
            pActiveContext->PrintDBEVersion();
            AgentErrorLog("Unsupported command packet error");
            break;

        case HSAIL_COMMAND_KILL_ALL_WAVES:
            AgentErrorLog("Unsupported command packet error");
            break;

        case HSAIL_COMMAND_CREATE_BREAKPOINT:
            DBECreateBreakpoint(pActiveContext, packet);
            break;

        case HSAIL_COMMAND_DISABLE_BREAKPOINT:
            DBEDisablePCBreakpoint(pActiveContext, packet);
            break;

        case HSAIL_COMMAND_DELETE_BREAKPOINT:
            DBEDeleteBreakpoint(pActiveContext, packet);
            break;

        case HSAIL_COMMAND_MOMENTARY_BREAKPOINT:
            DBEMomentaryBreakpoint(pActiveContext, packet);
            break;

        case HSAIL_COMMAND_CONTINUE:
            pActiveContext->m_ReadyToContinue = true;
            break;

        case HSAIL_COMMAND_ENABLE_BREAKPOINT:
            DBEEnablePCBreakpoint(pActiveContext, packet);
            break;

        case HSAIL_COMMAND_SET_LOGGING:
            pActiveContext->SetLogging(packet.m_loggingInfo);
            break;

        case HSAIL_COMMAND_UNKNOWN:
            pActiveContext->PrintDBEVersion();
            AgentErrorLog("Incomplete command packet error");
            break;

        default:
            pActiveContext->PrintDBEVersion();
            AgentErrorLog("Incomplete command packet error");
            break;
    }
}
