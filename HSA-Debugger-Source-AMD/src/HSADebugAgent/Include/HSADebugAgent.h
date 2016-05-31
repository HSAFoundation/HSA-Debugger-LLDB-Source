//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file   
/// \brief Functions to initialize the dispatch callbacks
//==============================================================================
#ifndef _HSA_DEBUG_AGENT_H_
#define _HSA_DEBUG_AGENT_H_

// Called from interception code set predispatch callbacks
HsailAgentStatus InitDispatchCallbacks(hsa_queue_t* queue);

// Called from interception code to shut down the AgentContext
void ShutDownHsaAgentContext(const bool skipDbeShutDown);

#endif // _HSA_DEBUG_AGENT_H
