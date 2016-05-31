//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file  AgentBreakpoint.h
/// \brief Agent breakpoint structure
//==============================================================================

#ifndef _AGENT_BREAKPOINT_H_
#define _AGENT_BREAKPOINT_H_

#include "AgentContext.h"
#include "AgentLogging.h"
#include "CommunicationControl.h"

namespace HwDbgAgent
{
/// GDB assigns a integer breakpoint id to each breakpoint
typedef int GdbBkptId;

const GdbBkptId g_UNKOWN_GDB_BKPT_ID = -9999;

typedef enum
{
    HSAIL_BREAKPOINT_STATE_UNKNOWN,   ///< We havent set breakpoint state yet
    HSAIL_BREAKPOINT_STATE_PENDING,   ///< HSAIL breakpoint received and not yet been created
    HSAIL_BREAKPOINT_STATE_DISABLED,  ///< HSAIL Breakpoint has been created but is disabled
    HSAIL_BREAKPOINT_STATE_ENABLED,   ///< HSAIL Breakpoint has been created and is enabled
} HsailBkptState;

typedef enum
{
    HSAIL_BREAKPOINT_TYPE_UNKNOWN,      ///< We havent set breakpoint type yet
    HSAIL_BREAKPOINT_TYPE_TEMP_PC_BP,   ///< This is a temp PC breakpoint
    HSAIL_BREAKPOINT_TYPE_PC_BP,        ///< A program counter breakpoint
    HSAIL_BREAKPOINT_TYPE_DATA_BP,      ///< A data breakpoint
    HSAIL_BREAKPOINT_TYPE_KERNEL_NAME_BP,///< A kernel name breakpoint
} HsailBkptType;


/// A single condition for a breakpoint.
/// This class is stored within a AgentBreakpoint.
/// The condition is evaluated by calling CheckCondition() calls
class AgentBreakpointCondition
{
public:
    AgentBreakpointCondition():
        m_workitemID(g_UNKNOWN_HWDBGDIM3),
        m_workgroupID(g_UNKNOWN_HWDBGDIM3),
        m_conditionCode(HSAIL_BREAKPOINT_CONDITION_ANY)
    {
        AGENT_LOG("Allocate an AgentBreakpointCondition");
    }

    ~AgentBreakpointCondition()
    {
    }

    /// Populate from the condition packet we get from GDB
    /// \param[in] The condition packet we get from GDB
    /// \return HSAIL agent status
    HsailAgentStatus SetCondition(const HsailConditionPacket& pCondition);

    /// Check the condition against a workgroup and workitem pair
    HsailAgentStatus CheckCondition(const HwDbgDim3 ipWorkGroup, const HwDbgDim3 ipWorkItem, bool& conditionCodeOut) const;

    /// Check the condition against a wavefront
    /// It is easier to just check the waveinfo structure directly.
    /// The checkCondition function can be implemented in multiple forms, with the SIMT model
    ///
    /// \param[in] pWaveInfo An entry from the waveinfo buffer
    /// \param[out] conditionCodeOut Return true if the condition matches
    /// \param[out] conditionTypeOut The type of condition.  Based on this type the focus control changes focus
    /// \return HSAIL agent status
    HsailAgentStatus CheckCondition(const HwDbgWavefrontInfo* pWaveInfo,
                                          bool&               conditionCodeOut,
                                          HsailConditionCode& conditionTypeOut) const;

    /// Get function to get the workgroup, used for FocusControl
    /// \return The work group used for this condition
    HwDbgDim3 GetWG() const;

    /// Get function to get the workitem, used for FocusControl
    /// \return The work item used for this condition
    HwDbgDim3 GetWI() const;

    /// Print the condition to the AGENT_OP
    void PrintCondition() const;

private:

    /// Disable copy constructor
    AgentBreakpointCondition(const AgentBreakpointCondition&);

    /// Disable assignment operator
    AgentBreakpointCondition& operator=(const AgentBreakpointCondition&);

    /// The work group used to match this condition
    HwDbgDim3 m_workitemID;

    /// The work item used to match this condition
    HwDbgDim3 m_workgroupID;

    /// The type of condition
    HsailConditionCode m_conditionCode;

};


/// A single HSAIL breakpoint, includes the GDB::DBE handle information
class AgentBreakpoint
{
public:

    /// The present state of the breakpoint
    /// \todo make this private since it should only be changed by the DBE functions
    HsailBkptState m_bpState;

    /// Number of times the BP was hit (unit reported in: wavefronts)
    int m_hitcount;

    /// The GDB IDs that map to this PC
    std::vector<GdbBkptId> m_GdbId;

    /// The PC we set the breakpoint on
    HwDbgCodeAddress m_pc;

    /// The type of the breakpoint
    HsailBkptType m_type;

    /// The line message that will be printed
    /// The constant g_HSAIL_BKPT_MESSAGE_LEN will limit the length of this string since
    /// it is sent within a packet from GDB
    std::string m_lineName;

    /// The HSAIL source line number
    int m_lineNum;

    /// Kernel name - used for function breakpoints
    std::string m_kernelName;

    /// The condition that will be checked for this breakpoint.
    /// Presently hsail-gdb supports only one condition at a time per breakable line
    AgentBreakpointCondition m_condition;

    AgentBreakpoint():
        m_bpState(HSAIL_BREAKPOINT_STATE_UNKNOWN),
        m_hitcount(0),
        m_GdbId(),
        m_pc(HSAIL_ISA_PC_UNKOWN),
        m_type(HSAIL_BREAKPOINT_TYPE_UNKNOWN),
        m_lineName("Unknown Line"),
        m_lineNum(-1),
        m_kernelName(""),
        m_condition(),
        m_handle(nullptr)
    {

        AGENT_LOG("Allocate an AgentBreakpoint");
    }

    /// Find a position in the payload and updates the payload  with its hitcount
    HsailAgentStatus UpdateNotificationPayload(HsailNotificationPayload* payload) const;

    /// Just print the appropriate message for the breakpoint type
    void PrintHitMessage() const;

    /// This function calls the DBE for the first time, otherwise it remembers the GDB ID
    /// \param[in] The DBE context handle
    /// \param[in] The GDB ID for this breakpoint
    /// \return HSAIL agent status
    HsailAgentStatus CreateBreakpointDBE(const HwDbgContextHandle DbeContextHandle,
                                         const GdbBkptId          gdbId = g_UNKOWN_GDB_BKPT_ID);

    /// Delete breakpoint - for kernel source
    /// This function deletes the breakpoint in DBE only if no more GDB IDs are left
    ///
    /// \param[in] The GDB ID that we want to delete for this breakpoint
    /// \return HSAIL agent status
    HsailAgentStatus DeleteBreakpointDBE(const HwDbgContextHandle dbeHandle,
                                         const GdbBkptId          gdbId = g_UNKOWN_GDB_BKPT_ID);

    /// Delete breakpoint - for kernel name
    /// \param[in] The GDB ID that we want to delete for this breakpoint
    /// \return HSAIL agent status
    HsailAgentStatus DeleteBreakpointKernelName(const GdbBkptId gdbID);

private:

    /// Disable copy constructor
    AgentBreakpoint(const AgentBreakpoint&);

    /// Disable assignment operator
    AgentBreakpoint& operator=(const AgentBreakpoint&);

    /// The BP handle for the DBE
    HwDbgCodeBreakpointHandle m_handle;
};
}
#endif // #ifndef _AGENT_BREAKPOINT_H_
