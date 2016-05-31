//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Class to print wave information
//==============================================================================
#ifndef _AGENT_WAVEPRINTER_H_
#define _AGENT_WAVEPRINTER_H_

#include <cstddef>
#include <vector>

#include "AMDGPUDebug.h"

namespace HwDbgAgent
{
const int g_KERNEL_DEBUG_WORKITEMS_PER_WAVEFRONT = 64;

/// This class mirrors the hdKernelDebugWavefront structure in CodeXL
/// This class should include the struct used to communicate with GDB
/// "HsailAgentWaveInfo m_WaveInfo;"
class AgentDbgWavefront
{
private:

    /// We don't use this constructor for now
    AgentDbgWavefront();

public:

    /// Constructor:
    AgentDbgWavefront(HwDbgCodeAddress wavefrontProgramCounter, HwDbgWavefrontAddress wavefrontAddress);

    /// Destructor
    ~AgentDbgWavefront();

    /// Access the work items in the wavefront:
    void GetWorkItemCoordinate(int index, int coord[3]) const;

    /// Query if a work item is present in the wavefront:
    bool ContainsWorkItem(const int coord[3]) const;

    /// Holds current PC upon break
    HwDbgCodeAddress m_wavefrontProgramCounter;
    /// Unique wave address
    HwDbgWavefrontAddress m_wavefrontAddress;
    /// Work item coordinates
    int m_workItemIds[3 * g_KERNEL_DEBUG_WORKITEMS_PER_WAVEFRONT];
};


/// This is a container for functions that tracks the active wave information
///
/// The main roles are
/// 1) Calls the DBE to get active waves and print the OP.
/// 2) Calls the DBE to get active waves and Send the waves to gdb
/// This class will need to always query the DBE for whatever waves are active,
/// This class is part of the agentcontext and the shared memory to send to gdb
/// is initialized and destroyed in this class
class AgentWavePrinter
{
private:

    std::vector<AgentDbgWavefront> m_currentWavefronts;

    /// The global and local OpenCL work sizes:
    int m_DispatchGlobalWorkDimensions;
    HsailWaveDim3 m_debuggedKernelHSAWorkgroupSize;

    void ClearCurrentWavefronts();

    /// Free the shared memory
    HsailAgentStatus FreeWaveInfoShmem();

    /// Allocate the shared memory
    void InitializeWaveInfoShmem();

    /// Private function that prints out data using the AgentOP() utility function
    HsailAgentStatus PrintWaveInfoBuffer(int nWaves, const HwDbgWavefrontInfo* pWaveInfo);

    /// Needs a DBE context handle and a event type and then calls private printwaveinfo
    HsailAgentStatus PrintActiveWaves(HwDbgEventType dbeEventType, HwDbgContextHandle pHandle);

public:
    AgentWavePrinter():
        m_currentWavefronts(),
        m_DispatchGlobalWorkDimensions(-1)// State is unknown initially
    {
        // We create the shared memory for the IPC
        InitializeWaveInfoShmem();

        AGENT_LOG("Initialize AgentWavePrinter");
    }

    ~AgentWavePrinter();


    /// Needs a DBE context handle and a event type and sends the active wave info to gdb
    HsailAgentStatus SendActiveWavesToGdb(HwDbgEventType dbeEventType, HwDbgContextHandle  debugHandle);
};

} // End Namespace HwDbgAgent

#endif // _AGENT_WAVEPRINTER_H_
