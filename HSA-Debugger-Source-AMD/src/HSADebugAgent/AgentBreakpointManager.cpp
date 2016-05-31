//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief The breakpoint manager class
//==============================================================================
#include <iostream>
#include <cassert>
// \todo including cstdint seems to need a -std=c++11 compiler switch on gcc
// Maye better to avoid C++11 for now.
#include <stdint.h>
#include <cstddef>
#include <fstream>
#include <string.h>
#include <errno.h>

// Use some stl vectors for maintaining breakpoint handles
#include <vector>

// Include the DBE
#include "AMDGPUDebug.h"

// Agent includes
#include "AgentBreakpoint.h"
#include "AgentBreakpointManager.h"
#include "AgentFocusWaveControl.h"

#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"


namespace HwDbgAgent
{
// Interfaces with the DBE for breakpoint tasks
// \input GDB breakpoint ID
/// pBreakpointPosOut -1 if the breakpoint does not exist or is disabled
/// \return false if the breakpoint does not exist or is disabled
bool AgentBreakpointManager::GetBreakpointFromGDBId(const GdbBkptId ipId,
                                                    int*     pBreakpointPosOut) const
{
    bool retVal = false;

    if (pBreakpointPosOut == nullptr)
    {
        AGENT_ERROR("pBreakpointPosOut is nullptr");
        return retVal;
    }

    *pBreakpointPosOut = -1;

    AGENT_LOG("GetBreakpointFromGDBId: Look For Breakpoint GDB ID: " << ipId);

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        AgentBreakpoint* pCurrentBP = m_pBreakpoints.at(i);
        if(pCurrentBP != nullptr)
        {
            for (unsigned int j = 0; j < pCurrentBP->m_GdbId.size(); j++)
            {
                AGENT_LOG("For Breakpoint: " << i << "\t GDB ID: " <<  pCurrentBP->m_GdbId.at(j) );
                if (pCurrentBP->m_GdbId.at(j) == ipId)
                {
                    *pBreakpointPosOut = i;
                    retVal = true;
                    break;
                }
            }
            if (retVal == true)
            {
                break;
            }
        }
    }

    if (retVal == false)
    {
        AGENT_ERROR("Could not find the breakpointID");
    }

    return retVal;
}


/// Get the position of the breakpoint which includes this PC
/// \todo maybe template the function GetBreakpointFrom* ?
/// pBreakpointPosOut -1 if the breakpoint does not exist or is disabled
/// \return false if the breakpoint does not exist or is disabled
bool AgentBreakpointManager::GetBreakpointFromPC(const HwDbgCodeAddress pc,
                                                       int*             pBreakpointPosOut,
                                                       bool*            pMomentaryBreakpointOut) const
{
    bool retVal = false;

    int bpIndex = -1;

    if (pBreakpointPosOut == nullptr)
    {
        AGENT_ERROR("pBreakpointPosOut is nullptr");
        return retVal;
    }

    bool pcbpFound = false;
    unsigned int bpCount = (unsigned int)m_pBreakpoints.size();

    for (unsigned int i = 0; i < bpCount; i++)
    {
        AgentBreakpoint* pCurrentBP = m_pBreakpoints.at(i);

        if (pCurrentBP->m_pc == pc)
        {
            // It should be enabled, otherwise something is very wrong
            if (pCurrentBP->m_bpState == HSAIL_BREAKPOINT_STATE_ENABLED ||
                pCurrentBP->m_bpState == HSAIL_BREAKPOINT_STATE_PENDING)
            {
                bpIndex = i;
                retVal = true;
                pcbpFound = true;
                break;
            }
        }
    }

    bool bpMomentary = false;

    if (!retVal)
    {
        unsigned int momentaryBPCount = (unsigned int)m_pMomentaryBreakpoints.size();

        for (unsigned int i = 0; i < momentaryBPCount; i++)
        {
            AgentBreakpoint* pCurrentBP = m_pMomentaryBreakpoints.at(i);

            if (pCurrentBP->m_pc == pc)
            {
                // Found the breakpoint:
                bpMomentary = true;
                bpIndex = i;
                retVal = true;
                break;
            }
        }
    }

    if (!pcbpFound && !bpMomentary)
    {
        AGENT_LOG("AgentBreakpointManager:GetBreakpointForPC, Couldnt find a mometary bp or a source pc");
    }

    *pBreakpointPosOut = bpIndex;
    *pMomentaryBreakpointOut = bpMomentary;

    return retVal;
}

// Returns true if the same PC exists in the breakpoint vector presently
bool AgentBreakpointManager::IsPCDuplicate(const HwDbgCodeAddress inputPC,
                                                 int&             duplicatePosition) const
{
    bool retCode = false;

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        AgentBreakpoint* bp = m_pBreakpoints.at(i);

        if (bp != nullptr)
        {
            if (bp->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP)
            {
                if (bp->m_pc == inputPC)
                {
                    AGENT_OP("HSAIL-GDB detected a duplicate breakpoint\n"
                            "Breakpoint " << bp->m_GdbId.at(0) << " already exists at PC 0x" << std::hex << inputPC << std::dec << "\n"
                            "Use breakpoint index " << bp->m_GdbId.at(0) << " "
                            "to enable/disable/delete breakpoints at PC 0x" << std::hex << inputPC << std::dec);

                    duplicatePosition = i;

                    retCode = true;
                    break;
                }
            }
        }
        else
        {
            // We should not have any nullptr elements in the vector, sine when we free
            // the AgentBreakpoint, we also remove it from m_pBreakpoints
            AGENT_ERROR("IsPCDuplicate: bp was nullptr");
        }
    }

    return retCode;
}

/// \return true iff there's a PC breakpoint on the value indicated
bool AgentBreakpointManager::IsPCBreakpoint(const HwDbgCodeAddress pc) const
{
    bool retVal = false;

    if (HSAIL_ISA_PC_UNKOWN != pc)
    {
        unsigned int bpCount = (unsigned int)m_pBreakpoints.size();

        for (unsigned int i = 0; i < bpCount; i++)
        {
            AgentBreakpoint* pCurrentBP = m_pBreakpoints.at(i);

            if (pCurrentBP->m_pc == pc)
            {
                // It should be enabled, otherwise something is very wrong
                if (pCurrentBP->m_bpState == HSAIL_BREAKPOINT_STATE_ENABLED)
                {
                    retVal = true;
                    break;
                }
                else
                {
                    // This logic assumes that each PC will be unique to a breakpoint
                    // That means that once a breakpoint is disabled, the PC should not be hit by the DBE
                    // We exit this loop if this happens since something is logically wrong here
                    break;
                }
            }
        }
    }

    return retVal;
}


/// The CreateBreakpoint does not check for duplicate breakpoint creation, since it is too
/// late to tell the user when it reaches the agent.  That will be the job of HwDbgFacilities
/// in the GDB side
HsailAgentStatus AgentBreakpointManager::CreateBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                                          const HsailCommandPacket ipPacket,
                                                          const HsailBkptType      ipType)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (ipPacket.m_command != HSAIL_COMMAND_CREATE_BREAKPOINT)
    {
        AGENT_ERROR("CreateBreakpoint: Function called for wrong packet type");
        return status;
    }

    // Check for duplicate source breakpoints
    int duplicatePosition;

    if (ipType == HSAIL_BREAKPOINT_TYPE_PC_BP)
    {
        if (IsPCDuplicate(ipPacket.m_pc, duplicatePosition))
        {
            AGENT_LOG("CreateBreakpoint: Detected a duplicate Kernel Source breakpoint\n" <<
                       "Append GDB ID " << ipPacket.m_gdbBreakpointID << " to m_GdbId vector");

            m_pBreakpoints.at(duplicatePosition)->m_GdbId.push_back(ipPacket.m_gdbBreakpointID);
            return HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    // Check for duplicate function breakpoints
    if (ipType == HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP)
    {
        std::string kernelName;
        kernelName.assign(ipPacket.m_kernelName);

        if (CheckAgainstKernelNameBreakpoints(kernelName, &duplicatePosition))
        {
            AGENT_LOG("CreateBreakpoint: Detected a duplicate Function breakpoint");
            AGENT_OP("HSAIL-GDB detected a duplicate function breakpoint") ;

            m_pBreakpoints.at(duplicatePosition)->m_GdbId.push_back(ipPacket.m_gdbBreakpointID);
            return HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    // If we reach here, we have a new breakpoint and we will need a AgentBreakpoint object

    // We need a valid DBE context for this call
    // This enforces our step in logic which says that we cannot create a kernel pc
    // breakpoint before we create a function breakpoint
    AgentBreakpoint* pBkpt = new(std::nothrow) AgentBreakpoint;

    if (pBkpt == nullptr)
    {
        AGENT_ERROR("CreateBreakpoint: Error in allocating AgentBreakpoint");
        return status;
    }


    pBkpt->m_type = ipType;

    // We need a code breakpoint
    // If this breakpoint has a PC, create it in DBE
    if (HSAIL_ISA_PC_UNKOWN != ipPacket.m_pc && nullptr != DbeContextHandle)
    {
        pBkpt->m_pc = (HwDbgCodeAddress)ipPacket.m_pc;
        pBkpt->m_condition.SetCondition(ipPacket.m_conditionPacket);

        status = pBkpt->CreateBreakpointDBE(DbeContextHandle, ipPacket.m_gdbBreakpointID);
        // If success we know that m_bpState = HSAIL_BREAKPOINT_STATE_ENABLED;
    }

    // We need a function breakpoint
    // We should have some valid data in the packet to assign
    if (ipPacket.m_kernelName != nullptr && ipType == HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP)
    {
        pBkpt->m_GdbId.push_back(ipPacket.m_gdbBreakpointID);

        if (ipPacket.m_kernelName[0] != '\0')
        {
            // Assigning with AGENT_MAX_FUNC_NAME_LEN causes the resulting std::string to have a size of AGENT_MAX_FUNC_NAME_LEN,
            // even if the actual string is shorter. Instead, we run a loop that is equivalent to a bounded version of strlen:
            // \todo Move this into the AgentBreakpoint class
            int len = 0;

            for (; AGENT_MAX_FUNC_NAME_LEN >= len && (char)0 != ipPacket.m_kernelName[len]; ++len);

            pBkpt->m_kernelName.assign(ipPacket.m_kernelName, len);
            pBkpt->m_bpState = HSAIL_BREAKPOINT_STATE_ENABLED;
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
    }

    // We don't print line information for temporary breakpoints
    // and kernel function breakpoints for now
    // We print line information for other types
    if (pBkpt->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP)
    {
        pBkpt->m_lineNum = ipPacket.m_lineNum;
        pBkpt->m_lineName.assign(ipPacket.m_sourceLine);
    }

    if (status == HSAIL_AGENT_STATUS_SUCCESS)
    {
        m_pBreakpoints.push_back(pBkpt);
    }
    else
    {
        delete pBkpt;
    }

    return status;
}

// This will be called when we close the agent context
HsailAgentStatus AgentBreakpointManager::ClearBreakpointVectors()
{
    // We just delete all elements in both breakpoint vectors

    // We cannot delete them in the DBE now since we wont have a context
    HsailAgentStatus status = HSAIL_AGENT_STATUS_SUCCESS;

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        if (m_pBreakpoints.at(i) != nullptr)
        {
            delete m_pBreakpoints.at(i);
            m_pBreakpoints.at(i) = nullptr;
        }
        else
        {
            AGENT_ERROR("ClearBreakpointVectors: nullptr elements in breakpoint vector");
            status = HSAIL_AGENT_STATUS_FAILURE;
        }
    }

    m_pBreakpoints.clear();

    for (unsigned int i = 0; i < m_pMomentaryBreakpoints.size(); i++)
    {
        if (m_pMomentaryBreakpoints.at(i) != nullptr)
        {
            delete m_pMomentaryBreakpoints.at(i);
            m_pMomentaryBreakpoints.at(i) = nullptr;
        }
        else
        {
            AGENT_ERROR("ClearBreakpointVectors: nullptr elements in Momentary breakpoint vector");
            status = HSAIL_AGENT_STATUS_FAILURE;
        }
    }

    m_pMomentaryBreakpoints.clear();
    return status;
}

HsailAgentStatus AgentBreakpointManager::DeleteBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                                          const HsailCommandPacket ipPacket)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // Validate packet is the right type
    if (ipPacket.m_command != HSAIL_COMMAND_DELETE_BREAKPOINT)
    {
        AGENT_ERROR("DeleteBreakpoint: Function called for wrong packet type");
        return status;
    }

    int breakpointpos = -1;

    bool isFound = false;
    isFound = GetBreakpointFromGDBId(static_cast<GdbBkptId>(ipPacket.m_gdbBreakpointID),
                                     &breakpointpos);

    if (!isFound)
    {
        AGENT_ERROR("AgentBreakpointManager: DeletePCBreakpoint, could not find GDB ID");
        return status;
    }

    if (breakpointpos == -1)
    {
        AGENT_ERROR("AgentBreakpointManager: DeletePCBreakpoint, invalid breakpoint position returned");
        return status;
    }

    AgentBreakpoint* pBkpt =  m_pBreakpoints.at(breakpointpos);

    if (pBkpt == nullptr)
    {
        AGENT_ERROR("AgentBreakpointManager: pBkpt is nullptr");
        return status;
    }


    if (pBkpt->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP)
    {
        // We do need to do the real delete only if there is no GDB ID left over
        // The DeleteBreakpointDBE handles this by looking in the GDB ID vector
        // and then calling the DBE accordingly
        status = pBkpt->DeleteBreakpointDBE(DbeContextHandle, ipPacket.m_gdbBreakpointID);

        if (status == HSAIL_AGENT_STATUS_FAILURE)
        {
            AGENT_ERROR("AgentBreakpointManager: Could not delete breakpoint in DBE");
            return status;
        }
    }

    if (pBkpt->m_type == HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP)
    {
        // We do need to do the real delete only if there is no GDB ID left over
        status = pBkpt->DeleteBreakpointKernelName(ipPacket.m_gdbBreakpointID);

        if (status == HSAIL_AGENT_STATUS_FAILURE)
        {
            AGENT_ERROR("AgentBreakpointManager: Could not delete kernel name breakpoint");
            return status;
        }
    }


    // Delete the memory and then remove the pointer from the breakpoint manager's vector
    // only if no more GDB  IDs map to this breakpoints
    if (m_pBreakpoints.at(breakpointpos)->m_GdbId.size() == 0)
    {
        delete m_pBreakpoints.at(breakpointpos);
        m_pBreakpoints.erase(m_pBreakpoints.begin() + breakpointpos);
    }
    else
    {
        AGENT_LOG ("Breakpoint at Position:" << breakpointpos << " not yet deleted from the Breakpoint manager");
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }
    // Breakpoint deleted in DBE and the vector has been updated
    return status;
}

HsailAgentStatus AgentBreakpointManager::DisableAllBreakpoints(HwDbgContextHandle dbeHandle)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // This is reasonable if the DBE Handle is nullptr since EndDebugging is called already
    if (dbeHandle == nullptr)
    {
        AGENT_LOG("DisableAllBreakpoints: DBE Handle is nullptr, cannot disable all breakpoints");
        status = HSAIL_AGENT_STATUS_SUCCESS;
        return status;
    }

    // We could iterate over each breakpoint and call the DeleteinDBE API, this seems cleaner
    HwDbgStatus dbeStatus = HwDbgDeleteAllCodeBreakpoints(dbeHandle);
    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        // A failure here is wrong, we should have a nullptr the handle and not get here
        // if debugging is finished
        AGENT_ERROR("DisableAllBreakpoints: Error calling DBE " <<
                    GetDBEStatusString(dbeStatus));
        return status;
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    bool successCheck = true;
    // Set the state for all breakpoints
    for (unsigned int i=0; i < m_pBreakpoints.size(); i++)
    {
        AgentBreakpoint* bp = m_pBreakpoints.at(i);
        if (bp != nullptr)
        {
            // It is deleted by the DBE, we dont want to change any other state for now
            bp->m_bpState = HSAIL_BREAKPOINT_STATE_PENDING;
        }
        else
        {
            AGENT_LOG("Attempting to disable all breakpoints, nullptr breakpoint at " << i);
            successCheck = false;
        }
    }

    // Set the state for all momentary breakpoints too
    for (unsigned int i=0; i < m_pMomentaryBreakpoints.size(); i++)
    {
        AgentBreakpoint* bp = m_pMomentaryBreakpoints.at(i);
        if (bp != nullptr)
        {
            // It is deleted by the DBE, we dont want to change any other state for now
            bp->m_bpState = HSAIL_BREAKPOINT_STATE_PENDING;
        }
        else
        {
            AGENT_LOG("Attempting to disable all momentary breakpoints, nullptr breakpoint at " << i);
            successCheck = false;
        }
    }

    if (!successCheck)
    {
        status = HSAIL_AGENT_STATUS_FAILURE;
    }
    return status;
}

// A bunch of possible options that can be added such as disable for 'X' occurences
// Our disable breakpoint logic is to delete the breakpoint in the DBE, while still
// holding on to the breakpoint and its hit count in the agent
HsailAgentStatus AgentBreakpointManager::DisablePCBreakpoint(HwDbgContextHandle DbeContextHandle,
                                                             HsailCommandPacket ipPacket)
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (ipPacket.m_command != HSAIL_COMMAND_DISABLE_BREAKPOINT)
    {
        AGENT_ERROR("DisablePCBreakpoint: Function called for wrong packet type");
        return status;
    }

    int breakpointpos = -1;

    bool isFound = false;
    isFound = GetBreakpointFromGDBId(static_cast<GdbBkptId>(ipPacket.m_gdbBreakpointID),
                                     &breakpointpos);

    if (!isFound)
    {
        AGENT_ERROR("DisablePCBreakpoint: Couldn't find breakpoint from gdb id");
        return status;
    }

    if (breakpointpos == -1)
    {
        AGENT_ERROR("DisablePCBreakpoint: Error in DisablePCBreakpoint");
        return status;
    }

    if (m_pBreakpoints.at(breakpointpos)->m_bpState != HSAIL_BREAKPOINT_STATE_ENABLED)
    {
        AGENT_ERROR("DisablePCBreakpoint: Disabling a breakpoint in DISABLED already or INVALID");
    }

    // Added for readability
    AgentBreakpoint* pBkpt = m_pBreakpoints.at(breakpointpos);

    assert(pBkpt != nullptr);

    if (pBkpt == nullptr)
    {
        AGENT_ERROR("Invalid breakpoint pointer");
        return status;
    }

    if (pBkpt->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP)
    {
        status = pBkpt->DeleteBreakpointDBE(DbeContextHandle, ipPacket.m_gdbBreakpointID);
    }

    // Breakpoint deleted in DBE, it shouldn't hit now when we call HwDbgContinue
    return status;

}

HsailAgentStatus AgentBreakpointManager::EnableAllMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle)
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    if (m_pMomentaryBreakpoints.empty() ||
        (GetNumMomentaryBreakpointsInState(HSAIL_BREAKPOINT_STATE_PENDING) == 0 &&
        GetNumMomentaryBreakpointsInState(HSAIL_BREAKPOINT_STATE_ENABLED) == 0))
    {
        AGENT_LOG("EnableAllMomentaryBreakpoints: No PC breakpoints available");
        status = HSAIL_AGENT_STATUS_SUCCESS;
        return status;
    }

    if (DbeContextHandle == nullptr)
    {
        AGENT_ERROR("EnableAllMomentaryBreakpoints: DBE Handle is nullptr");
        return status;
    }
    bool successCheck = true;
    for (unsigned int i = 0; i < m_pMomentaryBreakpoints.size(); i++)
    {
        AgentBreakpoint* pBp = m_pMomentaryBreakpoints.at(i);

        if (pBp == nullptr)
        {
            AGENT_ERROR("nullptr breakpoint in Breakpoint manager's vector");
            successCheck = false;
            break;
        }

        if (pBp->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP &&
            (pBp->m_bpState == HSAIL_BREAKPOINT_STATE_ENABLED ||
             pBp->m_bpState == HSAIL_BREAKPOINT_STATE_PENDING))
        {
            // We dont care about the GDB ID, we want to enable the breakpoint
            //
            status = pBp->CreateBreakpointDBE(DbeContextHandle);
            if (status != HSAIL_AGENT_STATUS_SUCCESS)
            {
                AGENT_LOG("EnableAllMomentaryBreakpoints: Could not enable momentary breakpoint " << i);
            }
        }
    }
    if (!successCheck)
    {
        AGENT_ERROR("EnableAllMomentaryBreakpoints: nullptr breakpoint in Breakpoint manager's vector");
        return HSAIL_AGENT_STATUS_FAILURE;
    }
    // We dont bother checking the above status for success here since debug facilities gives us
    // a bunch of PCs where there are no NOPs
    return HSAIL_AGENT_STATUS_SUCCESS;
}

HsailAgentStatus AgentBreakpointManager::EnableAllPCBreakpoints(const HwDbgContextHandle DbeContextHandle)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    bool successCheck = true;

    // Enable all the Momentary Breakpoints as well
    status = EnableAllMomentaryBreakpoints(DbeContextHandle);
    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_LOG("EnableAllPCBreakpoints: Could not enable the momentary breakpoints");
    }

    // there are no PC breakpoints available
    if (m_pBreakpoints.empty() ||
        (GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_ENABLED,
                                  HSAIL_BREAKPOINT_TYPE_PC_BP) == 0 &&
         GetNumBreakpointsInState(HSAIL_BREAKPOINT_STATE_PENDING,
                                  HSAIL_BREAKPOINT_TYPE_PC_BP) == 0))
    {
        AGENT_LOG("EnableAllPCBreakpoints: No PC breakpoints available");
        status = HSAIL_AGENT_STATUS_SUCCESS;
        successCheck = true;
        return status;
    }

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        AgentBreakpoint* pBp = m_pBreakpoints.at(i);

        if (pBp == nullptr)
        {
            AGENT_ERROR("nullptr breakpoint in Breakpoint manager's vector");
            successCheck = false;
            break;
        }

        if (pBp->m_type == HSAIL_BREAKPOINT_TYPE_PC_BP &&
            (pBp->m_bpState == HSAIL_BREAKPOINT_STATE_ENABLED ||
             pBp->m_bpState == HSAIL_BREAKPOINT_STATE_PENDING))
        {
            // We dont care about the GDB ID, we want to enable the breakpoint
            status = pBp->CreateBreakpointDBE(DbeContextHandle);

            if (status != HSAIL_AGENT_STATUS_SUCCESS)
            {
                AGENT_ERROR("EnableAllPCBreakpoints: Could not enable breakpoint in DBE");
                successCheck = false;
                status = HSAIL_AGENT_STATUS_FAILURE;
            }
        }
    }

    if (successCheck)
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        AGENT_ERROR("EnableAllPCBreakpoints: Could not enable existing PC breakpoints");
    }

    return status;
}

/// Enable the HSAIL breakpoint based on the GDB ID we got from the command packet
/// If already enabled, or still pending, do nothing and get out
/// A bunch of possible options that can be added such as disable for 'X' occurrences
/// Analogous to the disable breakpoint logic
HsailAgentStatus AgentBreakpointManager::EnablePCBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                                            const HsailCommandPacket ipPacket)
{

    if (ipPacket.m_command != HSAIL_COMMAND_ENABLE_BREAKPOINT)
    {
        AGENT_ERROR("AgentBreakpointManager: EnablePCBreakpoint called with an incorrect packet");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    int breakpointpos;
    bool isFound = false;
    isFound = GetBreakpointFromGDBId(static_cast<GdbBkptId>(ipPacket.m_gdbBreakpointID),
                                     &breakpointpos);

    if (!isFound)
    {
        AGENT_ERROR("EnablePCBreakpoint: Couldn't find breakpoint from gdb id");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // Added for readability
    AgentBreakpoint* pBkpt = m_pBreakpoints.at(breakpointpos);

    if (pBkpt == nullptr)
    {
        AGENT_ERROR("Invalid breakpoint pointer");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    assert(pBkpt != nullptr);

    if (pBkpt->m_type != HSAIL_BREAKPOINT_TYPE_PC_BP)
    {
        AGENT_ERROR("We Only support enable and disable on PC breakpoints");
        return HSAIL_AGENT_STATUS_FAILURE;

    }

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // If a bp is in state enabled or pending, we should  not complain
    // We should also not change its state.
    // We should just leave it as it is since either, it will switch to ENABLED when
    // we may resolve it or it is already enabled in the DBE
    if (pBkpt->m_bpState == HSAIL_BREAKPOINT_STATE_ENABLED ||
        pBkpt->m_bpState == HSAIL_BREAKPOINT_STATE_PENDING)
    {

        AGENT_LOG("Cannot enable a breakpoint that is already enabled");

        // This may not be a error, a silly user may create a
        // breakpoint and then send a enable command
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        status = pBkpt->CreateBreakpointDBE(DbeContextHandle, ipPacket.m_gdbBreakpointID);
    }

    // Breakpoint created in DBE and state is enabled
    return status;

}


/// Allocate the shared mem for the momentary breakpoints
HsailAgentStatus AgentBreakpointManager::AllocateMomentaryBPBuffer() const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    status = AgentAllocSharedMemBuffer(g_MOMENTARY_BP_BUFFER_SHMKEY, g_MOMENTARY_BP_BUFFER_MAXSIZE);

    return status;
}

/// Free the shared mem for the momentary breakpoints
HsailAgentStatus AgentBreakpointManager::FreeMomentaryBPBuffer() const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    status = AgentFreeSharedMemBuffer(g_MOMENTARY_BP_BUFFER_SHMKEY, g_MOMENTARY_BP_BUFFER_MAXSIZE);
    return status;

}

/// Create a momentary breakpoint
HsailAgentStatus AgentBreakpointManager::CreateMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle,
                                                                    const HsailCommandPacket ipPacket)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (ipPacket.m_command != HSAIL_COMMAND_MOMENTARY_BREAKPOINT)
    {
        return status;
    }

    int numMomentaryBp = 0;
    numMomentaryBp = ipPacket.m_numMomentaryBP;

    HsailMomentaryBP* pMomentaryBP = nullptr;
    pMomentaryBP = (HsailMomentaryBP*)AgentMapSharedMemBuffer(g_MOMENTARY_BP_BUFFER_SHMKEY, g_MOMENTARY_BP_BUFFER_MAXSIZE);

    if (pMomentaryBP == nullptr)
    {
        AGENT_ERROR("MomentaryBreakpoint: Could not get the shared mem");
        return status;
    }

    AGENT_LOG("MomentaryBreakpoint: Create " << numMomentaryBp << " momentary breakpoints");

    for (int i = 0; i < numMomentaryBp; i++)
    {
        // Bind this breakpoint's PC:
        // We need a valid DBE context for this call
        // This enforces our step in logic which says that we cannot create a kernel pc
        // breakpoint before we create a function breakpoint
        AgentBreakpoint* pBkpt = new(std::nothrow) AgentBreakpoint;

        if (pBkpt == nullptr)
        {
            AGENT_ERROR("MomentaryBreakpoint: Error in allocating AgentBreakpoint");
            return status;
        }

        HwDbgCodeAddress pc = pMomentaryBP[i].m_pc ;

        if (HSAIL_ISA_PC_UNKOWN != pc && nullptr != DbeContextHandle)
        {
            pBkpt->m_pc = (HwDbgCodeAddress)pc;
            pBkpt->m_type = HwDbgAgent::HSAIL_BREAKPOINT_TYPE_PC_BP;
            pBkpt->m_lineNum = pMomentaryBP[i].m_lineNum;
            status = pBkpt->CreateBreakpointDBE(DbeContextHandle);
        }

        if (status == HSAIL_AGENT_STATUS_SUCCESS)
        {
            pBkpt->m_bpState = HSAIL_BREAKPOINT_STATE_ENABLED;
        }

        // We don't have source line information for temporary breakpoints
        // and kernel function breakpoints for now
        // We print line information for other types

        if (status == HSAIL_AGENT_STATUS_SUCCESS)
        {
            m_pMomentaryBreakpoints.push_back(pBkpt);
        }
        else
        {
            delete pBkpt;
        }
    }

    // Clear memory after we are done
    memset(pMomentaryBP, 0, sizeof(HsailMomentaryBP)*numMomentaryBp);

    status = AgentUnMapSharedMemBuffer((void*)pMomentaryBP);
    return status;
}

/// Clear all momentary breakpoints
HsailAgentStatus AgentBreakpointManager::ClearMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // TODO: thread safety?
    unsigned int numberMomentaryBP = (unsigned int)m_pMomentaryBreakpoints.size();
    unsigned int failureCount = 0;

    for (unsigned int i = 0; i < numberMomentaryBP; i++)
    {
        // This assumes no nullptr breakpoints nor any
        AgentBreakpoint* pCurrentBP = m_pMomentaryBreakpoints.at(i);
        HwDbgCodeAddress currentPC = pCurrentBP->m_pc;

        // If there's a non-momentary breakpoint set at the same PC, don't clear the BP from the hardware

        if (currentPC != HSAIL_ISA_PC_UNKOWN  &&  !IsPCBreakpoint(currentPC))
        {
            status = pCurrentBP->DeleteBreakpointDBE(DbeContextHandle);

            if (status != HSAIL_AGENT_STATUS_SUCCESS)
            {
                AGENT_ERROR("ClearMomentaryBreakpoints: Could not delete DBE breakpoint.");
                failureCount++;
            }
        }

        delete pCurrentBP;
    }

    m_pMomentaryBreakpoints.clear();

    return (0 == failureCount) ? HSAIL_AGENT_STATUS_SUCCESS : HSAIL_AGENT_STATUS_FAILURE;
}

// Count the number of  momentary breakpoints in a particular state,
// The default HsailBkptType parameter assumes all momentary breakpoints are PC breakpoints
int AgentBreakpointManager::GetNumMomentaryBreakpointsInState(const HsailBkptState ipState, const HsailBkptType type) const
{
    int count = 0;

    for (unsigned int i = 0; i < m_pMomentaryBreakpoints.size(); i++)
    {
        const AgentBreakpoint* pCurrentBP = m_pMomentaryBreakpoints.at(i);

        if (nullptr != pCurrentBP)
        {
            if ((pCurrentBP->m_bpState == ipState) &&
                (type == pCurrentBP->m_type))
            {
                count = count + 1;
            }
        }
    }

    return count;
}
// Count the number of breakpoints in a particular state
int AgentBreakpointManager::GetNumBreakpointsInState(const HsailBkptState ipState, const HsailBkptType type) const
{
    int count = 0;

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        const AgentBreakpoint* pCurrentBP = m_pBreakpoints.at(i);

        if (nullptr != pCurrentBP)
        {
            if ((pCurrentBP->m_bpState == ipState) &&
                ((HSAIL_BREAKPOINT_TYPE_UNKNOWN == type) ||
                 (type == pCurrentBP->m_type)))
            {
                count = count + 1;
            }
        }
    }

    return count;
}

// Check for kernel name breakpoints:
// Return true if any breakpoints kernel name matches input kernel name argument
bool AgentBreakpointManager::CheckAgainstKernelNameBreakpoints(const std::string& kernelName, int* pBpPositionOut) const
{
    if (kernelName.empty())
    {
        return false;
    }

    if (pBpPositionOut == nullptr)
    {
        return false;
    }

    *pBpPositionOut = -1;

    for (unsigned int i = 0; i < m_pBreakpoints.size(); i++)
    {
        const AgentBreakpoint* pCurrentBP = m_pBreakpoints[i];

        if ((nullptr != pCurrentBP) &&
            (HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP == pCurrentBP->m_type))
        {
            const std::string& currentKernelBPName = pCurrentBP->m_kernelName;
            static const std::string wildCardKernelName = "*";

            if (kernelName == currentKernelBPName)
            {
                *pBpPositionOut = i;
                return true;
            }
            else if ((wildCardKernelName == currentKernelBPName) && (-1 == *pBpPositionOut))
            {
                // If we found the wildcard breakpoint, note its index, but keep searching for a name-specific breakpoint:
                *pBpPositionOut = i;
            }
        }
    }

    // If we found the wildcard, report success:
    if (-1 != *pBpPositionOut)
    {
        return true;
    }

    AGENT_LOG("No existing function breakpoint for kernel name:" << kernelName  << "\t"
              "String length "  << kernelName.size());

    return false;
}

void AgentBreakpointManager::PrintWaveInfo(const HwDbgWavefrontInfo* pWaveInfo, const HwDbgDim3* pFocusWI) const
{
    if (pWaveInfo == nullptr)
    {
        AGENT_ERROR("PrintWaveInfo: pWaveInfo is nullptr");
        return;
    }

    std::stringstream buffer;
    buffer.str("");
    buffer << "Wavefront ID: 0x" << std::hex << pWaveInfo->wavefrontAddress << std::dec << "\t";

    buffer << "Work-group: "
           << pWaveInfo->workGroupId.x << ", "
           << pWaveInfo->workGroupId.y << ", "
           << pWaveInfo->workGroupId.z << "\t";

    if (pFocusWI !=  nullptr)
    {
        buffer << "Work-item:  "
               << pFocusWI->x << ", "
               << pFocusWI->y << ", "
               << pFocusWI->z << "\t";
    }

    buffer << "Wave: " << HWDBG_WAVEFRONT_SIZE << " work-items from "
           << pWaveInfo->workItemId[0].x << ", "
           << pWaveInfo->workItemId[0].y << ", "
           << pWaveInfo->workItemId[0].z << "\n";

    AgentOP(buffer.str().c_str());
}

/// We print the wave if it was stopped at a breakpoint
/// This has been kept separate from UpdateStatistics for now since this could be very different
/// if we just want to print one wave (focus wave and stuff..)
///
/// Another alternative is we use the waveprinter class here so we dont need to call DBE again
/// \todo fb bug 11073: We cannot use this function for stopping at kernel function breakpoints
/// for now
HsailAgentStatus AgentBreakpointManager::PrintStoppedReason(const HwDbgEventType         DbeEventType,
                                                            const HwDbgContextHandle     DbeContextHandle,
                                                                  AgentFocusWaveControl* pFocusWaveControl,
                                                                  bool*                  pIsStopNeeded)
{
    HwDbgStatus status;
    const HwDbgWavefrontInfo* pWaveInfo ;
    uint32_t nWaves = 0;

    assert(DbeEventType == HWDBG_EVENT_POST_BREAKPOINT);

    if (DbeEventType != HWDBG_EVENT_POST_BREAKPOINT)
    {
        AGENT_ERROR("DBE: Invalid call to HwDbgGetActiveWaves. DBE not in post breakpoint state");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    if (pIsStopNeeded == nullptr || pFocusWaveControl == nullptr)
    {
        AGENT_ERROR("pIsStopNeeded or pFocusWaveControl are nullptr");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    status = HwDbgGetActiveWavefronts(DbeContextHandle, &pWaveInfo, &nWaves);

    assert(status == HWDBG_STATUS_SUCCESS);

    if (status != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("DBE: Error in HwDbgGetActiveWaves");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    if (pWaveInfo == nullptr || nWaves <= 0)
    {
        AGENT_ERROR("PrintStoppedReason: Invalid input parameters");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // A logic check, we should have atleast one valid breakpoint set
    bool checkSingleValidBreakpoint = false;

    // For all active waves, that we get from the DBE
    for (size_t i = 0; i < nWaves; i++)
    {
        // Is a breakpoint set at this PC ?
        int bpId = -1;
        bool isMomentary = false;
        bool isPCFound = GetBreakpointFromPC(pWaveInfo[i].codeAddress, &bpId, &isMomentary);

        // If yes: bpId will tell us which bp to update
        // Increment the respective breakpoint's hit count by 1
        // If no: We just continue in a harmless manner since not every PC reported by the waveinfo
        // data needs to be at a breakpoint location
        if (bpId != -1 && isPCFound == true)
        {
            // Choose the right breakpoint vector, could be momentary / user set PC breakpoint
            AgentBreakpoint* pHitBP = (!isMomentary) ? m_pBreakpoints.at(bpId) : m_pMomentaryBreakpoints.at(bpId);

            if (pHitBP == nullptr)
            {
                AGENT_ERROR("pHitBp at BP id " << bpId << " is nullptr");
                continue;
            }

            checkSingleValidBreakpoint = true;

            // If the focus doesnt match, check that the breakpoint is conditional or not
            bool checkCondition = false;
            HsailConditionCode condCode;
            pHitBP->m_condition.CheckCondition(&pWaveInfo[i], checkCondition, condCode);

            if (checkCondition)
            {

                // We need to change the focus only if it is a real conditional
                if (condCode != HSAIL_BREAKPOINT_CONDITION_ANY &&
                    condCode != HSAIL_BREAKPOINT_CONDITION_UNKNOWN)
                {
                    HwDbgDim3 focusWg = pHitBP->m_condition.GetWG();
                    HwDbgDim3 focusWi = pHitBP->m_condition.GetWI();
                    pFocusWaveControl->SetFocusWave(nullptr, &focusWg, &focusWi);
                }

                pHitBP->m_condition.PrintCondition();
                pHitBP->PrintHitMessage();
                *pIsStopNeeded = true;
                break;
            }
        }
    }

    if (!checkSingleValidBreakpoint)
    {
        AGENT_ERROR("PrintStoppedReason: No valid breakpoint information found for this stop");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    return HSAIL_AGENT_STATUS_SUCCESS;

}

// We update each breakpoint's hit count by one if one wave is at the breakpoint
HsailAgentStatus AgentBreakpointManager::UpdateBreakpointStatistics(const HwDbgEventType DbeEventType,
                                                                    const HwDbgContextHandle DbeContextHandle)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    HwDbgStatus dbeStatus;
    const HwDbgWavefrontInfo* pWaveInfo ;
    uint32_t nWaves = 0;

    assert(DbeEventType == HWDBG_EVENT_POST_BREAKPOINT);

    if (DbeEventType != HWDBG_EVENT_POST_BREAKPOINT)
    {
        AGENT_ERROR("DBE: Invalid call to HwDbgGetActiveWaves. DBE not in post breakpoint state");
        return status;
    }

    dbeStatus = HwDbgGetActiveWavefronts(DbeContextHandle, &pWaveInfo, &nWaves);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("DBE: Error in HwDbgGetActiveWaves");
        assert(dbeStatus == HWDBG_STATUS_SUCCESS);
        return status;
    }

    if (pWaveInfo == nullptr || nWaves <= 0)
    {
        AGENT_ERROR("UpdateBreakpointStatistics: Invalid input parameters");
        return status;
    }

    // Build a notification payload to tell gdb about what breakpoints were hit
    HsailNotificationPayload notifyPayload;
    memset(&notifyPayload, 0, sizeof(HsailNotificationPayload));

    notifyPayload.m_Notification = HSAIL_NOTIFY_BREAKPOINT_HIT;
    notifyPayload.payload.BreakpointHit.m_numActiveWaves = nWaves;

    // Clear memory so that we know which breakpoints were updated
    for (size_t i = 0; i < HSAIL_MAX_REPORTABLE_BREAKPOINTS; i++)
    {
        notifyPayload.payload.BreakpointHit.m_breakpointId[i] = -1;
    }

    // A logic check, we should have atleast one valid breakpoint set
    bool checkSingleValidBreakpoint = false;

    // For all active waves, that we get from the DBE
    for (size_t i = 0; i < nWaves; i++)
    {
        // Is a breakpoint set at this PC ?
        int bpId = -1;
        bool isMomentary = false;
        bool isPCFound = GetBreakpointFromPC(pWaveInfo[i].codeAddress, &bpId, &isMomentary);

        // If yes: bpId will tell us which bp to update
        // Increment the respective breakpoint's hit count by 1
        // If no: We just continue in a harmless manner since not every PC reported by the waveinfo
        // data needs to be at a breakpoint location
        if (bpId != -1 && isPCFound == true)
        {
            checkSingleValidBreakpoint = true;
            AgentBreakpoint* pHitBP = (!isMomentary) ? m_pBreakpoints.at(bpId) : m_pMomentaryBreakpoints.at(bpId);

            // Update the hitcount
            pHitBP->m_hitcount++;

            // Momentary breakpoints are not GDB breakpoints:
            if (!isMomentary)
            {
                // Save it to the payload
                status = pHitBP->UpdateNotificationPayload(&notifyPayload);

                if (status != HSAIL_AGENT_STATUS_SUCCESS)
                {
                    AGENT_ERROR("Could not update notification payload");
                    AGENT_ERROR("Breakpoint statistics are not correct");
                    break;
                }
            }
        }
    }

    if (!checkSingleValidBreakpoint)
    {
        AGENT_ERROR("UpdateBreakpointStatistics: Atleast one PC should have had a corresponding Breakpoint");
        AGENT_ERROR("We stopped even though we did not have a valid reason");
        return status;
    }

    status = AgentNotifyBreakpointHit(notifyPayload);
    return status;
}

HsailAgentStatus AgentBreakpointManager::ReportFunctionBreakpoint(const std::string& kernelFunctionName)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    int bpPosition;
    bool isFound  = false;
    isFound  = CheckAgainstKernelNameBreakpoints(kernelFunctionName, &bpPosition);

    if (!isFound || bpPosition == -1)
    {
        AGENT_LOG("ReportFunctionBreakpoint: Could not find a function valid breakpoint to report");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }

    if (nullptr == m_pBreakpoints.at(bpPosition))
    {
        AGENT_ERROR("The reported breakpoint location is nullptr");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;

    }

    if (HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP != m_pBreakpoints.at(bpPosition)->m_type)
    {
        AGENT_ERROR("The reported breakpoint should be a function breakpoint");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;

    }

    AGENT_OP("Breakpoint " << m_pBreakpoints.at(bpPosition)->m_GdbId.at(0) << " "
             << "at HSAIL Kernel, " << kernelFunctionName << "()");

    m_pBreakpoints.at(bpPosition)->m_hitcount += 1;

    HsailNotificationPayload notifyPayload;
    memset(&notifyPayload, 0, sizeof(HsailNotificationPayload));
    notifyPayload.m_Notification = HSAIL_NOTIFY_BREAKPOINT_HIT;

    // 0 is valid here since we wont have any started waves yet.
    notifyPayload.payload.BreakpointHit.m_numActiveWaves = 0;

    // Clear memory so that we know which breakpoints were updated
    for (size_t i = 0; i < HSAIL_MAX_REPORTABLE_BREAKPOINTS; i++)
    {
        notifyPayload.payload.BreakpointHit.m_breakpointId[i] = -1;
    }

    status = m_pBreakpoints.at(bpPosition)->UpdateNotificationPayload(&notifyPayload);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not update notification packet for function breakpoint");
        return status;
    }

    status = AgentNotifyBreakpointHit(notifyPayload);
    return status;

}

// At this stage we cannot clear breakpoints in the DBE, we just clear the vectors
AgentBreakpointManager::~AgentBreakpointManager()
{
    HsailAgentStatus status = ClearBreakpointVectors();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("~AgentBreakpointManager: Could not clear breakpoint vectors");
    }

    status = FreeMomentaryBPBuffer();

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("~AgentBreakpointManager: Could not free the shared mem buffer for momentary BP");
    }

    AGENT_LOG("~AgentBreakpointManager: Free Breakpoint Manager");
    // What other cleanup is needed ?
}

} // End Namespace HwDbgAgent
