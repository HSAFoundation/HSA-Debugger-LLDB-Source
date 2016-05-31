//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief  An agent implementation for hsa to inject debugger calls into
///         the application process.
//==============================================================================
// Headers for signals
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

// HSA headers
#include <hsa_api_trace.h>
#include <hsa_ext_amd.h>
#include <amd_hsa_tools_interfaces.h>

// Module in Common/Src/DynamicLibraryModule
#include "HSADebuggerRTModule.h"

// HSA Debug Agent headers and parameters for shmem and fifo
#include "AgentNotifyGdb.h"
#include "AgentLogging.h"
#include "AgentContext.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"
#include "CommandLoop.h"
#include "CommunicationParams.h"
#include "HSADebugAgent.h"
#include "HSAIntercept.h"

// HSA Callbacks
#include "PrePostDispatchCallback.h"


// A loader for the HSA runtime tools library
// This is a class similar to the loader in the DBE
class HSADebuggerRTLoader
{

public:
    HSADebuggerRTLoader():
        m_pDebuggerRTModule(nullptr)
    {
        AGENT_LOG("HSADebuggerRTLoader: Allocate runtime tools library loader");
    }

    ~HSADebuggerRTLoader()
    {
        AGENT_LOG("HSADebuggerRTLoader: Free the runtime tools library loader");
        delete m_pDebuggerRTModule;
        m_pDebuggerRTModule = nullptr;
    }

    /// Gets the HSA Tools Runtime Module, do the loading only once
    /// \return the HSA Tools Runtime Module
    HSADebuggerRTModule* CreateHSADebuggerRTModule()
    {
        if (nullptr == m_pDebuggerRTModule)
        {
            m_pDebuggerRTModule = new(std::nothrow) HSADebuggerRTModule();

            if (nullptr == m_pDebuggerRTModule || !m_pDebuggerRTModule->IsModuleLoaded())
            {
                AGENT_ERROR("HSADebuggerRTLoader: Unable to load runtime tools library");
            }
        }

        return m_pDebuggerRTModule;
    }

private:
    /// Module for the HSA Debugger Runtime
    HSADebuggerRTModule* m_pDebuggerRTModule;

};


static HSADebuggerRTLoader* psDebuggerRTLoader = nullptr;

// The AgentContext will be a global pointer
// This is needed since "OnUnload" does not get a argument
// We will delete the AgentContext object on unload
static HwDbgAgent::AgentContext* psAgentContext = nullptr;

static void InitAgentContext();

// This signal handler is needed since we pass SIGUSR1 to the inferior
// for debugging multithreaded programs
void tempHandleSIGUSR1(int signal)
{
    if (signal != SIGUSR1)
    {
        AGENT_ERROR("A spurious signal detected in initialization");
        AGENT_ERROR("We don't know what to do");
    }

    return;
}

void tempHandleSIGUSR2(int signal)
{
    if (signal != SIGUSR2)
    {
        AGENT_ERROR("A spurious signal detected when we expect SIGUSR2");
        AGENT_ERROR("We don't know what to do");
    }
    else
    {
        AGENT_LOG("tempHandleSIGUSR2: Handling SIGUSR2 by doing nothing");
    }

    return;
}


// We initialize the AgentContext object by calling this function
// on the library load and then pass it to the predispatch callback
static void CreateHsaAgentContext()
{
    if (psAgentContext != nullptr)
    {
        AGENT_ERROR("Cannot reinitialize the agent context");
        AgentFatalExit();
    }

    psAgentContext = new(std::nothrow) HwDbgAgent::AgentContext;

    if (psAgentContext == nullptr)
    {
        AGENT_ERROR("Could not create a AgentContext for debug");
        AgentFatalExit();
    }

}


// global flag to ensure initialization is done once
bool g_bInit = false;


void InitHsaAgent()
{
    if (!g_bInit)
    {

        AGENT_LOG("===== HSADebugAgent activated =====");
        HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

        signal(SIGUSR1, tempHandleSIGUSR1);

        // Add a do nothing handler on SIGUSR2.
        // Needed since HCC or the HSA runtime seem to mess with handlers
        signal(SIGUSR2, tempHandleSIGUSR2);

        status = InitFifoWriteEnd();

        if (status  != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Could not initialize the fifo write end");
        }

        status = InitFifoReadEnd();

        if (status  != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Could not initialize the fifo read end");
        }

        AGENT_LOG("===== Fifos initialized===== ");

        // Now that GDB has started, allocate the Agent Context object
        InitAgentContext();
        g_bInit = true;

    }
    else
    {
        AGENT_LOG("HSA Agent is already loaded");
    }
}



static void InitAgentContext()
{
    if (!g_bInit)
    {
        AGENT_LOG("===== Init AgentContext =====");

        // The agent state object is global here
        CreateHsaAgentContext();

        // We can change agentcontext's state once we have successfully set it as the
        // userarg for the predispatch function
        HsailAgentStatus agentStatus = psAgentContext->Initialize();

        if (agentStatus != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("g_pAgentContext returned an error.");
            return;
        }
    }
}

// This function is called from both, the intercepted hsa_shut_down
// and the OnUnload function, in the case the application does not call
// hsa_shut_down for some reason we hope that atleast UnLoad is called
// (doesnt seem to be the case for now though)
void ShutDownHsaAgentContext(const bool skipDbeShutDown)
{
    HsailAgentStatus status = psAgentContext->ShutDown(skipDbeShutDown);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("ShutDownHsaAgentContext: Could not close the AgentContext");
    }

}
// global flag to ensure cleanup is done once
bool g_bCleanUp = false;


// We clear up the AgentContext by deleting the object when the
// agent is unloaded
static void DeleteHsaAgentContext()
{
    if (!g_bCleanUp)
    {
        if (psAgentContext != nullptr)
        {
            delete psAgentContext;
            psAgentContext = nullptr;
        }
        else
        {
            AGENT_ERROR("Could not delete AgentContext");
        }

        g_bCleanUp = true;
        g_bInit = false;
    }
}

static void CloseCommunicationFifo()
{
    if (!g_bCleanUp)
    {
        // The unlinking  (deleted from the file-system) of the fifo's is
        // done in the gdb end since we know that linux_nat_close
        // will be called later than the CleanUpHsaAgent() functions.

        AGENT_LOG("CloseCommunicationFifo: HSADebugAgent Cleanup");
        // Close the Agent <== GDB fifo
        int readFifoDescriptor = GetFifoReadEnd();
        close(readFifoDescriptor);

        // Close the Agent ==> GDB fifo
        int writeFifoDescriptor = GetFifoWriteEnd();
        close(writeFifoDescriptor);
    }
}

// Check the version based on the provided by HSA runtime's OnLoad function.
// The logic is based on code in the HSA profiler (HSAPMCAgent.cpp).
// Return success if the versions match.
static HsailAgentStatus AgentCheckVersion(uint64_t runtimeVersion, uint64_t failedToolCount,
                                          const char* const* pFailedToolNames)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    static const std::string HSA_RUNTIME_TOOLS_LIB("libhsa-runtime-tools64.so.1");

    if (failedToolCount > 0 && runtimeVersion > 0)
    {
        if (pFailedToolNames != nullptr)
        {
            for (uint64_t i = 0; i < failedToolCount; i++)
            {
                if (pFailedToolNames[i] != nullptr)
                {
                    std::string failedToolName = std::string(pFailedToolNames[i]);

                    if (std::string::npos != failedToolName.find_last_of(HSA_RUNTIME_TOOLS_LIB))
                    {
                        AGENT_OP("hsail-gdb not enabled. Version mismatch between HSA runtime and "
                                 << HSA_RUNTIME_TOOLS_LIB);
                        AGENT_ERROR("Debug agent not enabled. Version mismatch between HSA runtime and "
                                    << HSA_RUNTIME_TOOLS_LIB);
                        return status;
                    }
                }
                else
                {
                    AGENT_ERROR("Debug agent not enabled," << HSA_RUNTIME_TOOLS_LIB
                                << "version could not be verified");
                    AGENT_ERROR("AgentCheckVersion: pFailedToolNames[i] is nullptr")
                    return status;

                }
            }
        }
        else
        {
            AGENT_ERROR("AgentCheckVersion: Could not verify version successfully");
        }
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

HsailAgentStatus InitDispatchCallbacks(hsa_queue_t* queue)
{
    AGENT_LOG("Setup the HSADebugAgent callbacks");
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (queue == nullptr)
    {
        AGENT_ERROR("Could not set the dispatch callbacks, the queue is nullptr");
        return status;
    }

    hsa_status_t hsaStatus ;
    hsaStatus = psDebuggerRTLoader->CreateHSADebuggerRTModule()
                ->ext_tools_set_callback_functions(queue,
                                                   HwDbgAgent::PreDispatchCallback,
                                                   HwDbgAgent::PostDispatchCallback);

    if (hsaStatus != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR(GetHsaStatusString(hsaStatus).c_str());
        AGENT_ERROR("hsa_ext_tools_set_callback_functions returns an error.");
        return status;
    }

    // Assign the agent state to the predispatchcallback's arguments
    hsaStatus = psDebuggerRTLoader->CreateHSADebuggerRTModule()
                ->ext_tools_set_callback_arguments(queue,
                                                   reinterpret_cast<void*>(psAgentContext),
                                                   nullptr);

    if (hsaStatus != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR(GetHsaStatusString(hsaStatus).c_str());
        AGENT_ERROR("hsa_ext_tools_set_callback_arguments returns an error.");
        return status;
    }

    status = HSAIL_AGENT_STATUS_SUCCESS;
    return status;
}

extern "C" bool OnLoad(ApiTable* pTable,
                       uint64_t runtimeVersion, uint64_t failedToolCount,
                       const char* const* pFailedToolNames)
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    status = AgentInitLogger();

    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not initialize Logging");
        return false;
    }

    // Start the DBE, this will initialize the DBE's internal Tools RT loaders
    HwDbgStatus dbeStatus = HwDbgInit(reinterpret_cast<void*>(pTable));

    if (dbeStatus  != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("HwDbgInit failed: DBE Status" << GetDBEStatusString(dbeStatus));
        return false;

    }

    AGENT_LOG("===== Load GDB Tools Agent=====");

    status = AgentCheckVersion(runtimeVersion, failedToolCount, pFailedToolNames);

    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Version mismatch");
        return false;
    }


    status = InitHsaCoreAgentIntercept(pTable);

    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Could not initialize dispatch tables");
        return false;
    }

    psDebuggerRTLoader = new(std::nothrow) HSADebuggerRTLoader;

    if (psDebuggerRTLoader == nullptr)
    {
        AGENT_ERROR("Could not initialize the Agent's HSA RT module");
        return false;
    }

    // Initialize the communication with GDB
    InitHsaAgent();

    AGENT_LOG("===== Finished Loading GDB Tools Agent=====");

    return true;
}


extern "C" void OnUnload()
{
    AGENT_LOG("===== Unload GDB Tools Agent=====");
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    status = HwDbgAgent::WaitForDebugThreadCompletion();

    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("OnUnload:Error waiting for the debug thread to complete");
    }

    // We skip the DBE shutdown when we try to shutdown the AgentContext
    // since the HSA tools RT may already have been unloaded.
    ShutDownHsaAgentContext(true);

    CloseCommunicationFifo();

    // The agentcontext object is global here since the unload function
    // does not have an UserArg parameter
    // We need to be sure that all debug is over before we call this
    DeleteHsaAgentContext();

    if (psDebuggerRTLoader == nullptr)
    {
        AGENT_ERROR("OnUnload:Could not delete the debugger RT loader");
    }
    else
    {
        delete psDebuggerRTLoader ;
    }

    status = AgentCloseLogger();

    if (status  != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("OnUnload:Could not close Logging");
    }

}
