//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Agent processing interface, this file should be renamed to DBEProcessPacket ?
//==============================================================================
#ifndef _AGENT_PROCESS_PACKET_H
#define _AGENT_PROCESS_PACKET_H

#include <cstdbool>
#include <hsa_ext_amd.h>

// Add DBE (Version decided by Makefile)
#include "AMDGPUDebug.h"

// HSA Agent includes
#include "CommunicationControl.h"


// Process agent packet. This function is called from the command loop.
// Within this function we break down the packet and call the appropriate
// functions in the file
void AgentProcessPacket(HwDbgAgent::AgentContext* pActiveContext,
                        const HsailCommandPacket& packet);

void SetEvaluatorActiveContext(HwDbgAgent::AgentContext* activeContext);

void SetKernelParametersBuffers(const hsa_kernel_dispatch_packet_t* pAqlPacket);

// Called from the expression evaluator to kill all waves and end debugging
void KillHsailDebug(bool isQuitIssued);

#endif // _AGENT_PROCESS_PACKET_H
