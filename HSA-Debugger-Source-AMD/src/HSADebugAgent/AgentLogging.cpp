//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Functions for Agent logging,both for error and tracing purposes
//==============================================================================
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <dlfcn.h>

#include <hsa_ext_amd.h>
#include "AMDGPUDebug.h"

#include "AgentBinary.h"
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "AgentVersion.h"
#include "CommunicationControl.h"

static bool WriteDLLPathToString(const std::string& dllName, std::string& msg);
static bool WriteDLLPath(const std::string& dllName);

// The logging is disabled by default, you can call
// "set hsail logging on" to start logging - will log to cout
//
// The logger can be enabled by env variables
//
// export HSA_DEBUG_ENABLE_AGENTLOG='stdout'  --> will write to cout
// export HSA_DEBUG_ENABLE_AGENTLOG='filename' --> will write to filename

/// This logger is not threadsafe
class AgentLogManager
{
private:
    std::string m_AgentLogFileName;
    std::ofstream m_opStream;

    std::string m_AgentLogPrefix;
    std::string m_debugSessionID;

    /// Disable copy constructor
    AgentLogManager(const AgentLogManager&);

    /// Disable assignment operator
    AgentLogManager& operator=(const AgentLogManager&);

    void LogVersionInfo();

    void SetLoggingFromEnvVar();

    // These logging functions take a message only as an input.
    // The functions simply add some prefix and print the message.
    // The cout flush functions are only helpful to try and dump output asap
    void WriteToStdOut(const char* message) const;
    void WriteToOutStream(const char* message) ;

public:
    bool m_EnableLogging ;
    bool m_EnableISADump;

    AgentLogManager():
        m_AgentLogFileName(""),
        m_opStream(),
        m_AgentLogPrefix(""),
        m_debugSessionID(""),
        m_EnableLogging(false),
        m_EnableISADump(false)
    {
        SetLoggingFromEnvVar();

        LogVersionInfo();
    }

    ~AgentLogManager();

    bool OpenLogFile();
    void CloseLogFile();

    const std::string GetDbeBinaryFileName() const;

    const std::string GetIsaStreamFileName() const;

    void SetDebugSessionID(const char* pAgentLogPrefix,
                           const char* pGdbSessionIDEnvVar);

    // May probably template this later
    void WriteLog(const char* message) ;
    void WriteLog(const HsailCommandPacket& incomingPacket);
};

// The file name is generated based on the prefix from the env variable, _CodeObject_ and session ID
const std::string AgentLogManager::GetDbeBinaryFileName() const
{
    std::string opFileName = m_AgentLogPrefix + "_CodeObject_" + m_debugSessionID + ".bin";
    return opFileName;
}

// The file name is generated based on the prefix from the env variable, ISA_Stream and session ID
const std::string AgentLogManager::GetIsaStreamFileName() const
{
    std::string opFileName = m_AgentLogPrefix + "_ISA_Stream_" + m_debugSessionID + ".log";
    return opFileName;
}

void AgentLogManager::LogVersionInfo()
{
    std::string infoStr = "";

    // HSAIL GDB
    infoStr = "HSAIL-GDB version: " + gs_HSAIL_GDB_VERSION + "\n";
    WriteLog(infoStr.c_str());

    // Debug Backend
    infoStr = "AMDGPUDebug version: "
              + std::to_string(AMDGPUDEBUG_VERSION_MAJOR) + "."
              + std::to_string(AMDGPUDEBUG_VERSION_MINOR) + "."
              + std::to_string(AMDGPUDEBUG_VERSION_BUILD) + ".\n";
    WriteLog(infoStr.c_str());
}


// Creates the SessionID_$Session_PID_$pid  string, will be reused for all agengLogging
void AgentLogManager::SetDebugSessionID(const char* pAgentLogPrefix,
                                        const char* pGdbSessionIDEnvVar)
{
    if (pAgentLogPrefix == nullptr || pGdbSessionIDEnvVar == nullptr)
    {
        AGENT_ERROR("SetDebugSessionID cannot be called with nullptr parameters");
    }

    m_AgentLogPrefix.assign(pAgentLogPrefix);

    std::stringstream buffer;
    buffer.str("");
    buffer << "SessionID_" << pGdbSessionIDEnvVar << "_PID_" << getpid();

    m_debugSessionID.assign(buffer.str());

}

// Set logging to a file from a env var
// export HSA_DEBUG_ENABLE_AGENTLOG = foo
// export HSA_DEBUG_ENABLE_AGENTLOG = stdout
void AgentLogManager::SetLoggingFromEnvVar()
{
    char* pLogNameEnvVar;
    pLogNameEnvVar = std::getenv("HSAIL_GDB_ENABLE_LOG");

    // This is an internal variable set by the hsail-gdb build script
    char* pGDBSessionEnVar;
    pGDBSessionEnVar = std::getenv("HSAIL_GDB_DEBUG_SESSION_ID");

    // This is only evaluated if logging is set, since the logging decides the filename
    char* pEnableISADumpEnVar;
    pEnableISADumpEnVar = std::getenv("HSAIL_GDB_ENABLE_ISA_DUMP");

    // We need both env variables
    if (pLogNameEnvVar != nullptr &&  pGDBSessionEnVar != nullptr)
    {
        SetDebugSessionID(pLogNameEnvVar, pGDBSessionEnVar);

        std::string opFileNamePrefix(pLogNameEnvVar);

        bool retCode = false;

        if (opFileNamePrefix == "stdout")
        {
            std::cout << "The AgentLog will print to stdout:\n";
            retCode = true;
        }
        else
        {
            retCode = OpenLogFile();
        }

        if (retCode)
        {
            m_EnableLogging = true;
        }

        if (retCode == true && pEnableISADumpEnVar != nullptr)
        {
            std::string enableISADump(pEnableISADumpEnVar);

            if (enableISADump == "1")
            {
                m_EnableISADump = true;
            }
            else
            {
                AGENT_ERROR("Invalid environment variable value for HSAIL_GDB_ENABLE_ISA_DUMP" <<
                            "export HSAIL_GDB_ENABLE_ISA_DUMP=1");
            }
        }

        AgentLogSetDBELogging(HWDBG_LOG_TYPE_ALL);
    }
}

void AgentLogManager::WriteLog(const char* message)
{
    if (m_EnableLogging == false)
    {
        return;
    }

    if (!m_opStream.is_open())
    {
        WriteToStdOut(message);
    }
    else
    {
        WriteToOutStream(message);
    }
}

void AgentLogManager::WriteLog(const HsailCommandPacket& incomingPacket)
{
    if (m_EnableLogging == false)
    {
        return;
    }

    m_opStream.flush();
    m_opStream << "AgentLOG> ";
    m_opStream << "ReadPacket: " << GetCommandTypeString(incomingPacket.m_command) ;
    m_opStream << "\t PC 0x" << std::hex << static_cast<unsigned long long>(incomingPacket.m_pc) << std::dec;
    m_opStream << "\t Kernel name " << (char*)incomingPacket.m_kernelName << std::endl;
    m_opStream.flush();
}

void AgentLogManager::WriteToOutStream(const char* message)
{
    m_opStream.flush();
    m_opStream << "AgentLOG> " << message;
    m_opStream.flush();
}

void AgentLogManager::WriteToStdOut(const char* message)const
{
    std::cout.flush();
    std::cout << "AgentLOG> " << message;
    std::cout.flush();
}


bool AgentLogManager::OpenLogFile()
{
    if (!m_opStream.is_open())
    {
        std::stringstream buffer;
        buffer.str("");

        buffer << m_AgentLogPrefix << "_AgentLog_" << m_debugSessionID << ".log";
        m_AgentLogFileName.assign(buffer.str());

        m_opStream.open(m_AgentLogFileName.c_str(), std::ofstream::app);

        if (m_opStream.is_open())
        {
            m_opStream << "Start AgentLOG \n";
            std::cout << "The AgentLog File is: " << m_AgentLogFileName << "\n";
            std::cout.flush();
        }
    }

    return m_opStream.is_open();

}

void AgentLogManager::CloseLogFile()
{
    if (m_opStream.is_open())
    {
        m_opStream.close();

        std::cout << "Close the AgentLog File: " << m_AgentLogFileName << "\n";
        std::cout.flush();

    }
}

AgentLogManager::~AgentLogManager()
{
    if (m_opStream.is_open())
    {
        m_opStream.close();
    }
}

static AgentLogManager* gs_pAgentLogManager;


HsailAgentStatus AgentInitLogger()
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    gs_pAgentLogManager = new(std::nothrow)AgentLogManager;

    if (gs_pAgentLogManager != nullptr)
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    AgentPrintLoadedDLL();
    return status;
}

HsailAgentStatus AgentCloseLogger()
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (gs_pAgentLogManager != nullptr)
    {
        gs_pAgentLogManager->CloseLogFile();

        delete gs_pAgentLogManager;
        gs_pAgentLogManager = nullptr;
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

void AgentSetLogging(const bool logFlag)
{
    if (gs_pAgentLogManager != nullptr)
    {
        gs_pAgentLogManager->m_EnableLogging = logFlag;
    }
}


// The message will add the endl always
void AgentLog(const char* message)
{
    if (gs_pAgentLogManager != nullptr)
    {
        gs_pAgentLogManager->WriteLog(message);
    }
}

// The message will add the endl always
void AgentOP(const char* message)
{
    std::cout.flush();
    std::cout << "[hsail-gdb]: " << message;
    std::cout.flush();
}

// Write Packet information
void AgentPrintPacketInfo(const HsailCommandPacket& incomingPacket)
{
    std::cout.flush();
    std::cout << "AgentLOG> ";
    std::cout << "ReadPacket: Type " << GetCommandTypeString(incomingPacket.m_command) ;
    std::cout << "\t PC " << std::hex << static_cast<unsigned long long>(incomingPacket.m_pc);
    std::cout << "\t Kernel name " << (char*)incomingPacket.m_kernelName << std::endl;
    std::cout.flush();
}


static void AgentLoggingCallback(void* pUserData,
                                 const HwDbgLogType type,
                                 const char* const  pMessage)
{
    if (pMessage != nullptr)
    {
        std::stringstream buffer;
        buffer.str("");
        buffer  << "DBE Message: " << pMessage << std::endl;
        AgentLog(buffer.str().c_str());
    }

    // to get rid of compiler warning
    if (pUserData != nullptr)
    {
        // Ignore this data, not set by test code
        AGENT_ERROR("pUser wasn't nullptr for some reason");
    }
}


HsailAgentStatus AgentLogSetDBELogging(const HwDbgLogType logtype)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    HwDbgStatus dbeStatus;

    dbeStatus = HwDbgSetLoggingCallback(logtype, AgentLoggingCallback, nullptr);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR("DBE Error while enabling logging, DBE status: " << GetDBEStatusString(dbeStatus));
        status = HSAIL_AGENT_STATUS_FAILURE;
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

void AgentLogAppendFinalizerOptions(std::string& finalizerOptions)
{
    if (gs_pAgentLogManager->m_EnableISADump)
    {
        // Just to check where this function is called from
        if(finalizerOptions.empty())
        {
            AGENT_ERROR("AgentLogAppendFinalizerOptions: Finalizer Options string is empty, "
                        "debug flags should have been added already")
        }

        const std::string dumpIsa=" -dump-isa";
        finalizerOptions.append(dumpIsa);

        AGENT_LOG("AgentLogAppendFinalizerOptions: Finalizer Options: \""<<finalizerOptions<<"\"");
    }

}

// Logging function to save the CodeObject to a file and save ISA stream to a file
void AgentLogSaveBinaryToFile(const HwDbgAgent::AgentBinary* pBinary, hsa_kernel_dispatch_packet_t*  pAqlPacket)
{

    if (gs_pAgentLogManager->m_EnableLogging)
    {
        HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

        status = pBinary->WriteBinaryToFile(gs_pAgentLogManager->GetDbeBinaryFileName().c_str());

        if (status != HSAIL_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("AgentLogSaveBinaryToFile: Could not save binary");
        }

        if (gs_pAgentLogManager->m_EnableISADump)
        {
            status = AgentWriteISAToFile(gs_pAgentLogManager->GetIsaStreamFileName(),
                                         pAqlPacket);

            if (status != HSAIL_AGENT_STATUS_SUCCESS)
            {
                AGENT_ERROR("AgentLogSaveBinaryToFile: Could not save ISA Stream");
            }
        }
    }
}

void AgentLogAQLPacket(const hsa_kernel_dispatch_packet_t*  pAqlPacket)
{
    if (pAqlPacket == nullptr)
    {
        AGENT_LOG("===Start AQL Packet===" << "\n" <<
                  "nullptr AQL Packet" <<
                  "===End AQL Packet===")
        return;
    }

    AGENT_LOG("===Start AQL Packet===" << "\n"
              "header \t\t"                     << pAqlPacket->header << "\n" <<
              "setup \t\t"                      << pAqlPacket->setup << "\n" <<
              "workgroup_size_x \t\t"           << pAqlPacket->workgroup_size_x << "\n" <<
              "workgroup_size_y \t\t"           << pAqlPacket->workgroup_size_y << "\n" <<
              "workgroup_size_z \t\t"           << pAqlPacket->workgroup_size_z << "\n" <<
              "reserved0 \t\t"                  << pAqlPacket->reserved0 << "\n" <<
              "grid_size_x \t\t"                << pAqlPacket->grid_size_x << "\n" <<
              "grid_size_y \t\t"                << pAqlPacket->grid_size_y << "\n" <<
              "grid_size_z \t\t"                << pAqlPacket->grid_size_z << "\n" <<
              "private_segment_size \t\t"       << pAqlPacket->private_segment_size << "\n" <<
              "group_segment_size \t\t"         << pAqlPacket->group_segment_size << "\n" <<
              "kernel_object \t\t"              << pAqlPacket->kernel_object << "\n" <<
              "kernarg_address \t\t"            << pAqlPacket->kernarg_address << "\n" <<
              "reserved2 \t\t"                  << pAqlPacket->reserved2 << "\n" <<
              "completion_signal.handle \t\t"   << pAqlPacket->completion_signal.handle << "\n" <<
              "===End AQL Packet===");

}

// Write Packet information
void AgentLogPacketInfo(const HsailCommandPacket& incomingPacket)
{
    gs_pAgentLogManager->WriteLog(incomingPacket);
}

bool WriteDLLPathToString(const std::string& dllName, std::string& msg)
{
    // Same as struct link_map in <link.h>
    typedef struct LinkMap
    {
        void* pAddr;    //Difference between the address in the ELF file and the addresses in memory.
        char* pPath;    // Absolute file name object was found in.
        void* pLd;      // Dynamic section of the shared object.
        struct LinkMap* pNext, *pPrev; // Chain of loaded objects.
    } LinkMap;

    bool ret = false;
    void* hModule = nullptr;
    char* status = nullptr;

    dlerror(); // clear error status before processing
    hModule = dlopen(dllName.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    status = dlerror();

    if (nullptr != hModule)
    {
        // Get the path
        LinkMap* pLm = reinterpret_cast<LinkMap*>(hModule);
        msg.assign(pLm->pPath);
        msg += "\t Loaded";

        dlclose(hModule);
        ret = true;
    }
    else
    {
        if (nullptr != status)
        {
            msg += dllName + "\t Not Loaded (error " + status + ")";
        }
        else
        {
            msg += dllName + "\t Not Loaded (can be expected)";
        }

        ret = false;
    }

    return ret;
}

// Write DLL to agent log
bool WriteDLLPath(const std::string& dllName)
{
    std::string msg;
    bool ret = WriteDLLPathToString(dllName, msg);
    msg += "\n";
    gs_pAgentLogManager->WriteLog(msg.c_str());
    return ret;
}

// Check whether relevant DLLs are loaded and print to stdout
void AgentPrintLoadedDLL()
{
    WriteDLLPath("libhsa-runtime64.so.1");
    WriteDLLPath("libhsa-ext-finalize64.so.1");
    WriteDLLPath("libhsa-ext-image64.so.1");
    WriteDLLPath("libhsa-runtime-tools64.so.1");
    WriteDLLPath("libhsakmt.so.1");

    WriteDLLPath("libhsaild.so");
}


// The message will add the endl always
void AgentErrorLog(const char* message)
{
#ifdef LOG_ERR_TO_STDERR
    std::cerr.flush();
    std::cerr << "Error: Agent:" << message;
    std::cerr.flush();

    AgentLog(message);

#else
    HSAIL_UNREFERENCED_PARAMETER(message);
#endif
}

void AgentWarningLog(const char* message)
{
#ifdef LOG_ERR_TO_STDERR
    std::cerr.flush();
    std::cerr << "Warning: Agent:" << message;
    std::cerr.flush();

    AgentLog(message);

#else
    HSAIL_UNREFERENCED_PARAMETER(message);
#endif
}
