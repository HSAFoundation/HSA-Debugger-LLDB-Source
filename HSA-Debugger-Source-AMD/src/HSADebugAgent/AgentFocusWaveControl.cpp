//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Controlling the focus thread behavior and notifying gdb
//==============================================================================
#include "AMDGPUDebug.h"

#include "AgentContext.h"
#include "AgentFocusWaveControl.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentUtils.h"

#include "CommunicationControl.h"

namespace HwDbgAgent
{

HsailAgentStatus AgentFocusWaveControl::NotifyFocusWaveSwitch()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    // The comparison below is to do a sanity check that we dont print
    // the Unknown parameters
    if (!CompareHwDbgDim3(m_focusWorkGroup, g_UNKNOWN_HWDBGDIM3) &&
        !CompareHwDbgDim3(m_focusWorkItem, g_UNKNOWN_HWDBGDIM3))
    {
        AGENT_OP("Switching to work-group ("
                 << m_focusWorkGroup.x << "," << m_focusWorkGroup.y << "," << m_focusWorkGroup.z
                 << ") and work-item ("
                 << m_focusWorkItem.x << "," << m_focusWorkItem.y << "," << m_focusWorkItem.z
                 << ")");

        AGENT_LOG("Switching to work-group ("
                  << m_focusWorkGroup.x << "," << m_focusWorkGroup.y << "," << m_focusWorkGroup.z
                  << ") and work-item ("
                  << m_focusWorkItem.x << "," << m_focusWorkItem.y << "," << m_focusWorkItem.z
                  << ")");

        status = AgentNotifyFocusChange(m_focusWorkGroup, m_focusWorkItem);
    }
    else
    {
        AGENT_OP("Unknown focus wave");
    }

    return status;
}

// Function used by variable printing
HsailAgentStatus AgentFocusWaveControl::GetFocus(HwDbgDim3& opWg, HwDbgDim3& opWi) const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (CompareHwDbgDim3(m_focusWorkGroup, g_UNKNOWN_HWDBGDIM3))
    {
        AGENT_LOG("AgentFocusWaveControl: Returning an unknown focus wg");
        CopyHwDbgDim3(opWg, m_focusWorkGroup);

    }
    else if (CompareHwDbgDim3(m_focusWorkItem, g_UNKNOWN_HWDBGDIM3))
    {
        AGENT_LOG("AgentFocusWaveControl: Returning an unknown focus wi");
        CopyHwDbgDim3(opWg, m_focusWorkItem);
    }
    else
    {
        CopyHwDbgDim3(opWg, m_focusWorkGroup);
        CopyHwDbgDim3(opWi, m_focusWorkItem);
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

// Return true if the wave includes the focus workgroup and workitem
bool AgentFocusWaveControl::CompareFocusWave(const HwDbgWavefrontInfo* pWaveInfo) const
{
    bool retVal = false;

    if (pWaveInfo == nullptr)
    {
        AGENT_ERROR("CompareFocusWave: pWaveInfo is nullptr");
    }
    else
    {
        // If this wave includes the workgroup and workitem, return true
        retVal = AgentIsWorkItemPresentInWave(m_focusWorkGroup, m_focusWorkItem, pWaveInfo);
    }

    return retVal;

}

// If the function has an input, just overwrite the member objects.
// If the function has no input, the following describes the behavior:
//
// If focus is unknown choose choose the first entry from the waveinfo buffer
//
// If focus wave is known, check if it is in the waveinfo buffer,
// If present in buffer:     dont change the focus, get out of here, you're done
// If not present in buffer: choose the first wave from the buffer
HsailAgentStatus AgentFocusWaveControl::SetFocusWave(const HwDbgContextHandle dbeHandle,
                                                     const HwDbgDim3* pWg, const HwDbgDim3* pWi)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    const HwDbgWavefrontInfo* pWaveInfo ;
    uint32_t nWaves = 0;

    // Some input focus wave has been given (mostly from gdb)
    if ((pWg != nullptr) && (pWi != nullptr) && (dbeHandle == nullptr))
    {
        AGENT_LOG("SetFocusWave: Set the focus to the input parameter");

        // An optimization, we only need to update gdb if it changed
        if (!CompareHwDbgDim3(m_focusWorkGroup, pWg[0]) ||
            !CompareHwDbgDim3(m_focusWorkItem, pWi[0]))
        {
            CopyHwDbgDim3(m_focusWorkGroup, pWg[0]);
            CopyHwDbgDim3(m_focusWorkItem, pWi[0]);

            status = NotifyFocusWaveSwitch();
        }
        else
        {
            status = HSAIL_AGENT_STATUS_SUCCESS;
        }

        return status;

    }

    // Only one parameter being nullptr is not valid
    // This function should be called with no parameters or 2 valid parameters
    if ((pWg != nullptr && pWi == nullptr) ||
        (pWg == nullptr && pWi != nullptr))
    {
        AGENT_ERROR("SetFocusWave: called with one nullptr parameter");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }

    HwDbgStatus dbeStatus;
    dbeStatus = HwDbgGetActiveWavefronts(dbeHandle, &pWaveInfo, &nWaves);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("SetFocusWave: Error in SetFocusWave " << GetDBEStatusString(dbeStatus));

        assert(dbeStatus == HWDBG_STATUS_SUCCESS);
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // No active waves present to change focus for
    if (nWaves == 0 || pWaveInfo == nullptr)
    {
        AGENT_ERROR("SetFocusWave: WaveInfo buffer is nullptr, cannot choose a wave to set focus");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }

    // No focus wave known case, we just use the first one
    if (CompareHwDbgDim3(m_focusWorkGroup, g_UNKNOWN_HWDBGDIM3) ||
        CompareHwDbgDim3(m_focusWorkItem, g_UNKNOWN_HWDBGDIM3))
    {
        CopyHwDbgDim3(m_focusWorkGroup, pWaveInfo[0].workGroupId);
        CopyHwDbgDim3(m_focusWorkItem, pWaveInfo[0].workItemId[0]);

        status = NotifyFocusWaveSwitch();
        return status;

    }

    // The focus wg and wi are known
    bool isFocusWIFound = false;
    uint32_t i, j;

    for (i = 0; i < nWaves; i++)
    {
        if (CompareHwDbgDim3(pWaveInfo[i].workGroupId, m_focusWorkGroup) == true)
        {
            for (j = 0; j < HWDBG_WAVEFRONT_SIZE; j++)
            {
                if (CompareHwDbgDim3(pWaveInfo[i].workItemId[j], m_focusWorkItem) == true)
                {
                    isFocusWIFound = true;
                    AGENT_LOG("SetFocusWave: Found the focus wave in WaveInfo buffer");
                    status = HSAIL_AGENT_STATUS_SUCCESS;
                    break;
                }
            }
        }

        if (isFocusWIFound)
        {
            break;
        }
    }

    // We only need to update the member data for the focus if the focus WI was not
    // found in the waveinfo buffer.
    // If the focus WI was found, do nothing
    if (!isFocusWIFound)
    {
        AGENT_LOG("SetFocusWave: Could not find the focus wave, copying the first entry on WaveInfo buffer");
        CopyHwDbgDim3(m_focusWorkGroup, pWaveInfo[0].workGroupId);
        CopyHwDbgDim3(m_focusWorkItem, pWaveInfo[0].workItemId[0]);
        status = NotifyFocusWaveSwitch();
    }
    else
    {
        AGENT_LOG("SetFocusWave: The focus wave for the AgentContext has not been changed");
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}
}
