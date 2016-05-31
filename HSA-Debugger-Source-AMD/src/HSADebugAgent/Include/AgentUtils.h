//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Utility functions for HSA Debug Agent
//==============================================================================
#ifndef _AGENT_UTILS_H_
#define _AGENT_UTILS_H_

#include <stdint.h>
#include <cstddef>
#include <vector>

#include <hsa.h>

#include "AMDGPUDebug.h"

// A fatal exit function so that all the exit behavior can be handled in one place
void AgentFatalExit();

// A simple utility function to write the binary from DBE to a file
void WriteBinaryToFile(const void*  pBinary,
                       size_t binarySize,
                       const char*  pszFilename = "kernel.bin");

// Get symbols from an ELF binary
void ExtractSymbolListFromELFBinary(const void* pBinary,
                                    size_t binarySize,
                                    std::vector<std::pair<std::string, uint64_t>>& outputSymbols);

const std::string GetDBEEventString(const HwDbgEventType event);

const std::string GetDBEStatusString(const HwDbgStatus status);

const std::string GetCommandTypeString(const HsailCommand ipCommand);

std::string GetHsaStatusString(const hsa_status_t s);

// Checks if the workgroup and workitem passed belong to the wavefront
bool AgentIsWorkItemPresentInWave(const HwDbgDim3&          workGroup,
                                  const HwDbgDim3&          workItem,
                                  const HwDbgWavefrontInfo* pWaveInfo);

HsailAgentStatus AgentWriteISAToFile(const std::string&                  isaFileName,
                                           hsa_kernel_dispatch_packet_t* aql);

bool CompareHwDbgDim3(const HwDbgDim3& op1, const HwDbgDim3& op2);

void CopyHwDbgDim3(HwDbgDim3& dst, const HwDbgDim3& src);

// Disassemble the kernel and save it to the filename passed
#if 0
HsailAgentStatus AgentDisassembleHsailKernel(void* pAclBinary, std::string& kernelSource);

HsailAgentStatus  AgentWriteHsailKernelToFile(const std::string kernelSource,
                                              const std::string filename = "temp.hsail");
#endif

#endif // _AGENT_UTILS_H
