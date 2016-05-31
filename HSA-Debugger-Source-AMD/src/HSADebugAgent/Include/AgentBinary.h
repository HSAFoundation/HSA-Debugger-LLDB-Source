//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Implementation of AgentBinary
//==============================================================================
#ifndef _AGENT_BINARY_H_
#define _AGENT_BINARY_H_

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <sstream>

#include <hsa.h>

#include "AMDGPUDebug.h"
#include "CommunicationControl.h"

namespace HwDbgAgent
{

// forward declaration:
class AgentContext;

// A class that maintains a single binary from the DBE
class AgentBinary
{
private:

    /// Binary from DBE - the memory for the binary is allocated by the DBE
    const void* m_pBinary;

    /// Size of binary
    size_t m_binarySize;

    /// Symbols we dig out of the binary
    std::string m_llSymbolName;
    std::string m_hlSymbolName;
    std::string m_kernelName;

    /// Dispatch dimensions workgroup size
    HsailWaveDim3 m_workGroupSize;

    /// Dispatch dimensions workitem size
    HsailWaveDim3 m_gridSize;

    /// Disable copy constructor
    AgentBinary(const AgentBinary&);

    /// Disable assignment operator
    AgentBinary& operator=(const AgentBinary&);

    /// Get the HL and LL symbols
    bool GetDebugSymbolsFromBinary();

    bool PopulateKernelNameFromBinary(const hsa_kernel_dispatch_packet_t* pAqlPacket);

    /// Write the binary to shared mem
    HsailAgentStatus WriteBinaryToSharedMem(const key_t shmKey) const;

public:
    AgentBinary():
        m_pBinary(nullptr),
        m_binarySize(0),
        m_llSymbolName(""),
        m_hlSymbolName(""),
        m_kernelName(""),
        m_workGroupSize({0,0,0}),
        m_gridSize({0,0,0})
    {

    }

    ~AgentBinary();

    /// Call HwDbgGetShaderBinary and populate the object.
    /// Also extracts the relevant symbols from the binary.
    ///
    /// \param[in] ipHandle The DBE context handle
    /// \param[in] pAqlPacket The AQL packet for the dispatch
    /// \return HSAIL agent status
    HsailAgentStatus PopulateBinaryFromDBE(HwDbgContextHandle ipHandle, const hsa_kernel_dispatch_packet_t* pAqlPacket);

    /// Get the workgroup and workgroup dimension from the agent context
    /// The data there was defined from Aqlpacket when begin debug.
    ///
    /// \param[in] pAgentContext The debug agent context for this HSA application
    /// \return HSAIL agent status
    void PopulateWorkgroupSizeInformation(AgentContext* pAgentContext);

    /// Write the Notification payload to gdb
    HsailAgentStatus NotifyGDB() const;

    /// Write the binary to a file, useful for debug
    /// \param[in] pFilenamePrefix Input filename prefix
    HsailAgentStatus WriteBinaryToFile(const char* pFilenamePrefix) const;

    /// Return the kernel name
    ///
    /// \return The kernel name for this code object
    const std::string GetKernelName() const;
};
} // End Namespace HwDbgAgent

#endif // _AGENT_BINARY_H_
