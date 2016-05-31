//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Header for the breakpoint manager class
//==============================================================================
#ifndef _AGENT_BREAKPOINT_MANANGER_H_
#define _AGENT_BREAKPOINT_MANANGER_H_

#include <cstddef>

// Relevant STL
#include <vector>
#include <sstream>

#include "AMDGPUDebug.h"
#include "AgentBreakpoint.h"
#include "AgentLogging.h"
#include "CommunicationControl.h"
#include "CommunicationParams.h"

namespace HwDbgAgent
{

class AgentBreakpoint;

/// A class that works with HSAIL packets and DBE context information
/// and maintains a vector of AgentBreakpoint
/// The original implementation of this class was two vectors (m_BreakpointHandleList) and
/// (m_BreakpointGdbIdList) which would stay in sync. The two Breakpoint handle and the GDB
/// ID are now part of the AgentBreakpoint class
/// This class will maintain the breakpoint and source line information for a single kernel
class AgentBreakpointManager
{
private:

    /// A vector of breakpoint pointers
    std::vector<AgentBreakpoint*> m_pBreakpoints;

    /// A vector of momentary breakpoint pointers
    std::vector<AgentBreakpoint*> m_pMomentaryBreakpoints;

    /// Name of the file where the hsail kernel source is saved
    std::string m_kernelSourceFilename;

    /// Allocate the shared mem for the momentary breakpoints
    HsailAgentStatus AllocateMomentaryBPBuffer() const;

    /// Free the shared mem for the momentary breakpoints
    HsailAgentStatus FreeMomentaryBPBuffer() const;

    /// Called internally when we need a new temp breakpoint
    GdbBkptId CreateNewTempBreakpointId();

    /// To clear up memory when we destroy the breakpoint manager.
    HsailAgentStatus ClearBreakpointVectors();

    /// Enable all the momentary breakpoints, done as part of the EnableAllPCBreakpoints
    HsailAgentStatus EnableAllMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle);

    /// \return position of the breakpoint object - based on PC
    bool GetBreakpointFromPC(const HwDbgCodeAddress pc, int* pBreakpointPosOut, bool* pMomentaryBreakpointOut) const;

    /// \return true iff there's a PC breakpoint on the value indicated
    bool IsPCBreakpoint(const HwDbgCodeAddress pc) const;

    bool IsPCDuplicate(const HwDbgCodeAddress m_pc, int& duplicatePosition) const;

    /// \return position of the breakpoint object - based on GDBID
    bool GetBreakpointFromGDBId(const GdbBkptId ipId, int* pBreakpointPosOut) const;

    /// Utility function to print the wave info for the breakpoint we just hit
    void PrintWaveInfo(const HwDbgWavefrontInfo* pWaveInfo, const HwDbgDim3* pFocusWI = nullptr) const;

    /// Disable copy constructor
    AgentBreakpointManager(const AgentBreakpointManager&);

    /// Disable assignment operator
    AgentBreakpointManager& operator=(const AgentBreakpointManager&);

public:

    AgentBreakpointManager():
        m_kernelSourceFilename("temp_source")
    {
        HsailAgentStatus status = AllocateMomentaryBPBuffer();

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Could not initialize the shared mem buffer for momentary BP");
        }
        else
        {
            AGENT_LOG("Successfully Initialized Breakpoint Manager");
        }
    }

    ~AgentBreakpointManager();

    /// Take in the context and an input packet and create a breakpoint
    HsailAgentStatus CreateBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                      const HsailCommandPacket ipPacket,
                                      const HsailBkptType ipType);

    /// Take in the context and an input packet and delete the breakpoint
    HsailAgentStatus DeleteBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                      const HsailCommandPacket ipPacket);

    /// This function is added to check the need for a temporary breakpoint.
    /// Needed for a step in if the developer has not set a breakpoint already
    /// Based on the number of enabled breakpoints, developer can call the temp bkptp API
    /// "type" filters on the breakpoint type, unless it's set to unknown, in which case
    /// it will return all breakpoints in the state:
    int GetNumBreakpointsInState(const HsailBkptState ipState, const HsailBkptType type = HSAIL_BREAKPOINT_TYPE_UNKNOWN) const;

    /// Count the number of momentary breakpoints
    int GetNumMomentaryBreakpointsInState(const HsailBkptState ipState, const HsailBkptType type = HSAIL_BREAKPOINT_TYPE_PC_BP) const;

    /// Returns true iff there is a kernel name breakpoint set against the input parameter name
    bool CheckAgainstKernelNameBreakpoints(const std::string& kernelName, int* pBpPositionOut) const;

    /// Disable a breakpoint
    HsailAgentStatus DisablePCBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                         const HsailCommandPacket ipPacket);

    /// Disable all breakpoints (source and momentary) by deleting them in the DBE
    HsailAgentStatus DisableAllBreakpoints(HwDbgContextHandle dbeHandle);

    /// Enable all breakpoints, needed for when we are in the predispatch callback
    /// We need to call the DBE for each breakpoint to enable them
    HsailAgentStatus EnableAllPCBreakpoints(const HwDbgContextHandle DbeContextHandle);

    /// Enable a breakpoint
    HsailAgentStatus EnablePCBreakpoint(const HwDbgContextHandle DbeContextHandle,
                                        const HsailCommandPacket ipPacket);

    /// Create a momentary breakpoint
    HsailAgentStatus CreateMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle,
                                                const HsailCommandPacket ipPacket);

    /// Clear all momentary breakpoints
    HsailAgentStatus ClearMomentaryBreakpoints(const HwDbgContextHandle DbeContextHandle);

    /// Checks the eventtype and then checks all the breakpoint PCs against the active wave PCs
    HsailAgentStatus PrintStoppedReason(const HwDbgEventType     DbeEventType,
                                        const HwDbgContextHandle DbeContextHandlel,
                                              AgentFocusWaveControl* pFocusWaveControl,
                                              bool*                  pIsStopNeeded);

    /// Update the hit count of each breakpoint based on what we get from GetActiveWaves
    /// EventType is passed just to check that the DBE is in the right state before calling
    HsailAgentStatus UpdateBreakpointStatistics(const HwDbgEventType DbeEventType,
                                                const HwDbgContextHandle DbeContextHandle);

    /// Update the breakpoint statistics for kernel function breakpoints
    HsailAgentStatus ReportFunctionBreakpoint(const std::string& kernelFunctionName);

};

} // End Namespace HwDbgAgent

#endif // _AGENT_BREAKPOINT_MANANGER_H_
