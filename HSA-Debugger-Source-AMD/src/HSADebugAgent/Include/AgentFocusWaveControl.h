//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Controlling the focus thread behavior and notifying gdb
//==============================================================================
#ifndef _AGENT_FOCUS_CONTROL_H_
#define _AGENT_FOCUS_CONTROL_H_

#include "AMDGPUDebug.h"
#include "AgentLogging.h"

namespace HwDbgAgent
{

class AgentFocusWaveControl
{
public:
    AgentFocusWaveControl():
        m_focusWorkGroup(g_UNKNOWN_HWDBGDIM3),
        m_focusWorkItem(g_UNKNOWN_HWDBGDIM3)
    {
        AGENT_LOG("Initialize AgentFocusWaveControl ");
    }
    ~AgentFocusWaveControl() {}

    bool CompareFocusWave(const HwDbgWavefrontInfo* pWaveInfo) const;

    // Function used by breakpoint manager and other components
    HsailAgentStatus SetFocusWave(const HwDbgContextHandle dbeHandle,
                                  const HwDbgDim3* pWg,
                                  const HwDbgDim3* pWi);

    // Function used by variable printing
    HsailAgentStatus GetFocus(HwDbgDim3& pWg,
                              HwDbgDim3& pWi) const;

private:
    /// Disable copy constructor
    AgentFocusWaveControl(const AgentFocusWaveControl&);

    /// Disable assignment operator
    AgentFocusWaveControl& operator=(const AgentFocusWaveControl&);


    // Notifies gdb about the focus change
    HsailAgentStatus NotifyFocusWaveSwitch();

    // Focus workgroup and workitem
    HwDbgDim3 m_focusWorkGroup;
    HwDbgDim3 m_focusWorkItem;
};
}

#endif //_AGENT_FOCUS_CONTROL_H_
