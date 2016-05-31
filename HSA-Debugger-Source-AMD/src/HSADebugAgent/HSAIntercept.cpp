//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Intercepting HSA Signal management calls and track validity of the signal
//==============================================================================

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <hsa.h>
#include <hsa_api_trace.h>
#include <hsa_ext_amd.h>
#include <hsa_ext_finalize.h>

#include <amd_hsa_tools_interfaces.h>

#include "AgentLogging.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"
#include "HSADebugAgent.h"
#include "HSAIntercept.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// const representing the min required queue size for SoftCP mode
/// This value is copied from HSAPMCInterceptionHelpers.h in the profiler
static const uint32_t MIN_QUEUE_SIZE_FOR_SOFTCP = 128;

// The HSA Runtime's versions of HSA API functions
ApiTable g_OrigCoreApiTable;

// The HSA Tools Runtime versions of the tools RT functions
ExtTable g_OrigHSAExtTable;

hsa_status_t
HsaDebugAgent_hsa_shut_down()
{
     AGENT_LOG("Interception: hsa_shut_down");

     ShutDownHsaAgentContext(false);

    hsa_status_t rtStatus;
    rtStatus = g_OrigCoreApiTable.hsa_shut_down_fn();

    // Note:
    // The AGENT_LOG statements below this will not usually print since we
    // the HSA RT presently calls the UnLoad function in the hsa_shut_down
    // which in turn closes AGENT_LOG.
    // If that behavior changes, we should see the corresponding Interception: Exit entry

    if (rtStatus != HSA_STATUS_SUCCESS)
     {
         AGENT_ERROR("Interception: Error in hsa_shut_down " << GetHsaStatusString(rtStatus));
         return rtStatus;
     }

    AGENT_LOG("Interception: Exit hsa_shut_down");

    return rtStatus;
}


hsa_status_t
HsaDebugAgent_hsa_queue_create(hsa_agent_t agent,
                               uint32_t size,
                               hsa_queue_type_t type,
                               void (*callback)(hsa_status_t status, hsa_queue_t* source,
                                                void* data),
                               void* data,
                               uint32_t private_segment_size,
                               uint32_t group_segment_size,
                               hsa_queue_t** queue)
{
    AGENT_LOG("Interception: hsa_queue_create");

    hsa_status_t rtStatus;

    // SoftCP mode requires a queue to be able to handle at least 128 packets
    if (MIN_QUEUE_SIZE_FOR_SOFTCP > size)
    {

        AGENT_OP("HSAIL-GDB is overriding the queue size passed to hsa_queue_create."
                 << "Queues must have a size of at least "
                 << MIN_QUEUE_SIZE_FOR_SOFTCP << " for debug.");

        AGENT_LOG("HSAIL-GDB is overriding the queue size passed to hsa_queue_create."
                  << "Queues must have a size of at least "
                  << MIN_QUEUE_SIZE_FOR_SOFTCP << " for debug.");

        size = MIN_QUEUE_SIZE_FOR_SOFTCP;
    }

    rtStatus = g_OrigCoreApiTable.hsa_queue_create_fn(agent,
                                                      size,
                                                      type,
                                                      callback,
                                                      data,
                                                      private_segment_size,
                                                      group_segment_size,
                                                      queue);

    if (rtStatus != HSA_STATUS_SUCCESS || *queue == nullptr)
    {
        AGENT_ERROR("Interception: Could not create a valid Queue, debugging will not work" <<
                    GetHsaStatusString(rtStatus));
        return rtStatus;
    }

    HsailAgentStatus status = InitDispatchCallbacks(*queue);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Could not configure queue for debug");
        // I am inclined to make rtStatus an error, we plainly want to debug but cant do so
    }

    AGENT_LOG("Interception: Exit hsa_queue_create");

    return rtStatus;

}


hsa_status_t
HsaDebugAgent_hsa_ext_program_finalize(hsa_ext_program_t            program,
                                       hsa_isa_t                    isa,
                                       int32_t                      call_convention,
                                       hsa_ext_control_directives_t control_directives,
                                       const char*                        options,
                                       hsa_code_object_type_t       code_object_type,
                                       hsa_code_object_t*           code_object)
{
    AGENT_LOG("Interception: hsa_ext_program_finalize");

    hsa_status_t status;
    std::string finalizerOptions;

    // Copy the existing input options if there are any
    if (options != nullptr)
    {
        finalizerOptions.assign(options);
    }

    // Append our options at the end, if there are any exiting options, add a space first
    if (!finalizerOptions.empty())
    {
        std::string blank(" ");
        finalizerOptions.append(blank);
    }

    // \todo: The finalizer string seems to be really sensitive with spaces and stuff.
    // \todo: Check for duplicates of debug options

    // Add debug options
    std::string debugOptions = "-g -O0 -amd-reserved-num-vgprs=4";
    finalizerOptions.append(debugOptions);

    // Add ISA logging options and any others if requested
    AgentLogAppendFinalizerOptions(finalizerOptions);

    AGENT_LOG("Interception: Options for finalizer:" << finalizerOptions);

    status = g_OrigHSAExtTable.hsa_ext_program_finalize_fn(program,
                                                           isa,
                                                           call_convention,
                                                           control_directives,
                                                           finalizerOptions.c_str(),
                                                           code_object_type,
                                                           code_object);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: HSA Runtime could not finalize the kernel " <<
                    GetHsaStatusString(status));
    }

    AGENT_LOG("Interception: Exit hsa_ext_program_finalize");

    return status;

}

static void UpdateHSAFunctionTable(ApiTable* pCoreTable)
{
    if (pCoreTable == nullptr)
    {
        return;
    }

    AGENT_LOG("Interception: Replace functions with HSADebugAgent versions");

    pCoreTable->hsa_queue_create_fn = HsaDebugAgent_hsa_queue_create;
    pCoreTable->hsa_shut_down_fn    = HsaDebugAgent_hsa_shut_down;

    pCoreTable->std_exts_->hsa_ext_program_finalize_fn = HsaDebugAgent_hsa_ext_program_finalize;
}


// This function will be extended with the kernel compilation interception too
HsailAgentStatus InitHsaCoreAgentIntercept(ApiTable* pTable)
{

    AGENT_LOG("InitHsaCoreAgentIntercept: Read HSA API Table");

    if (pTable == nullptr)
    {
        AGENT_ERROR("InitHsaCoreAgentIntercept: HSA Runtime provided a nullptr API Table");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // This saves the original pointers
    memcpy(static_cast<void*>(&g_OrigCoreApiTable),
           static_cast<const void*>(pTable),
           sizeof(ApiTable));

    memcpy(static_cast<void*>(&g_OrigHSAExtTable),
           static_cast<void*>(pTable->std_exts_),
           sizeof(ExtTable));

    // We override the table that we get from the runtime
    UpdateHSAFunctionTable(pTable);

    AGENT_LOG("InitHsaCoreAgentIntercept: Finished updating HSA API Table");
    return HSAIL_AGENT_STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus
