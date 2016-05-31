//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file  AgentUtils.cpp
/// \brief Utility functions for HSA Debug Agent
//==============================================================================
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include <stdint.h>
#include <cstddef>

#include <amd_hsa_kernel_code.h>

#include "AMDGPUDebug.h"

#include "AgentLogging.h"
#include "AgentUtils.h"
#include "CommunicationParams.h"

#include <libelf.h>

// Just a simple function so that all the exit behavior can be handled in one place
// We can add logging parameters but it is expected that you would call the logging
// functions before you fatally exit
// We will try to restrict this function's usage so that the process  dies only from
// errors in system calls
void AgentFatalExit()
{
    AgentErrorLog("FatalExit\n");
    exit(-1);
}

// return true if equal, else return false
bool CompareHwDbgDim3(const HwDbgDim3& op1, const HwDbgDim3& op2)
{
    if (op1.x == op2.x &&
        op1.y == op2.y &&
        op1.z == op2.z)
    {
        return true;
    }

    return false;
}

void CopyHwDbgDim3(HwDbgDim3& dst, const HwDbgDim3& src)
{
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
}

/// Just print the packet type
const std::string GetCommandTypeString(const HsailCommand ipCommand)
{
    switch (ipCommand)
    {
        case HSAIL_COMMAND_UNKNOWN:
            return "HSAIL_COMMAND_UNKOWN";

        case HSAIL_COMMAND_BEGIN_DEBUGGING:
            return "HSAIL_COMMAND_BEGIN_DEBUGGING";

        case HSAIL_COMMAND_KILL_ALL_WAVES:
            return "HSAIL_COMMAND_KILL_ALL_WAVES";

        case HSAIL_COMMAND_CREATE_BREAKPOINT:
            return "HSAIL_COMMAND_CREATE_BREAKPOINT";

        case HSAIL_COMMAND_DELETE_BREAKPOINT:
            return "HSAIL_COMMAND_DELETE_BREAKPOINT";

        case HSAIL_COMMAND_ENABLE_BREAKPOINT:
            return "HSAIL_COMMAND_ENABLE_BREAKPOINT";

        case HSAIL_COMMAND_DISABLE_BREAKPOINT:
            return "HSAIL_COMMAND_DISABLE_BREAKPOINT";

        case HSAIL_COMMAND_MOMENTARY_BREAKPOINT:
            return "HSAIL_COMMAND_MOMENTARY_BREAKPOINT";

        case HSAIL_COMMAND_CONTINUE:
            return "HSAIL_COMMAND_CONTINUE";

        case HSAIL_COMMAND_SET_LOGGING:
            return "HSAIL_COMMAND_CONTINUE";

        default:
            return "[Unknown Command]";
    }
}

/// Just print the DBE event
const std::string GetDBEEventString(const HwDbgEventType event)
{
    switch (event)
    {
        case HWDBG_EVENT_POST_BREAKPOINT:
            return "HWDBG_EVENT_POST_BREAKPOINT";

        case HWDBG_EVENT_TIMEOUT:
            return "HWDBG_EVENT_TIMEOUT";

        case HWDBG_EVENT_END_DEBUGGING:
            return "HWDBG_EVENT_END_DEBUGGING";

        case HWDBG_EVENT_INVALID:
            return "HWDBG_EVENT_INVALID";

        default:
            return "Unknown HWDBG_EVENT";
    }
}

/// Just print the DBE string
const std::string GetDBEStatusString(const HwDbgStatus status)
{
    switch (status)
    {
        case HWDBG_STATUS_SUCCESS:
            return "DBE Status: HWDBG_STATUS_SUCCESS\n";

        case HWDBG_STATUS_ERROR:
            return "DBE Status: HWDBG_STATUS_ERROR\n";

        case HWDBG_STATUS_DEVICE_ERROR:
            return "DBE Status: HWDBG_STATUS_DEVICE_ERROR\n";

        case HWDBG_STATUS_INVALID_HANDLE:
            return "DBE Status: HWDBG_STATUS_INVALID_HANDLE\n";

        case HWDBG_STATUS_INVALID_PARAMETER:
            return "DBE Status: HWDBG_STATUS_INVALID_PARAMETER\n";

        case HWDBG_STATUS_NULL_POINTER:
            return "DBE Status: HWDBG_STATUS_NULL_POINTER\n";

        case HWDBG_STATUS_OUT_OF_MEMORY:
            return "DBE Status: HWDBG_STATUS_OUT_OF_MEMORY\n";

        case HWDBG_STATUS_OUT_OF_RESOURCES:
            return "DBE Status: HWDBG_STATUS_OUT_OF_RESOURCES\n";

        case HWDBG_STATUS_REGISTRATION_ERROR:
            return "DBE Status: HWDBG_STATUS_REGISTRATION_ERROR\n";

        case HWDBG_STATUS_UNDEFINED:
            return "DBE Status: HWDBG_STATUS_UNDEFINED\n";

        case HWDBG_STATUS_UNSUPPORTED:
            return "DBE Status: HWDBG_STATUS_UNSUPPORTED\n";

        // This should never happen since we covered the whole enum
        default:
            return "DBE Status: [Unknown DBE Printing]";
    }
}

std::string GetHsaStatusString(const hsa_status_t s)
{
    const char* pszbuff = { 0 };
    hsa_status_string(s, &pszbuff);

    std::string str = pszbuff;
    return str;
}

// \todo: make it return HsailAgentStatus
void WriteBinaryToFile(const void*  pBinary,
                       size_t binarySize,
                       const char*  pszFilename)
{
    if (pBinary == nullptr)
    {
        AgentErrorLog("WriteBinaryToFile: Error Binary is null\n");
        return;
    }

    if (binarySize <= 0)
    {
        AgentErrorLog("WriteBinaryToFile: Error Binary size is invalid\n");
        return;
    }

    FILE* pFd = fopen(pszFilename, "wb");

    if (pFd == nullptr)
    {
        AgentErrorLog("WriteBinaryToFile: Error opening file\n");
        assert(!"Error opening file\n");
        return;
    }

    size_t retSize = fwrite(pBinary, sizeof(char), binarySize, pFd);

    fclose(pFd);

    if (retSize != binarySize)
    {
        AgentErrorLog("WriteBinaryToFile: Error writing to file\n");
        assert(!"WriteBinaryToFile: Error: fwrite failure.");
    }
}



bool AgentIsWorkItemPresentInWave(const HwDbgDim3&          workGroup,
                                  const HwDbgDim3&          workItem,
                                  const HwDbgWavefrontInfo* pWaveInfo)
{
    if (pWaveInfo == nullptr)
    {
        AgentErrorLog("AgentIsWorkItemPresentInWave: Waveinfo buffer is nullptr\n");
        return false;
    }

    bool isWgFound = false;
    bool isWiFound = false;


    if (CompareHwDbgDim3(pWaveInfo->workGroupId, workGroup))
    {
        isWgFound = true;
    }

    if (isWgFound)
    {
        for (int i = 0; i < HWDBG_WAVEFRONT_SIZE; i++)
        {
            if (CompareHwDbgDim3(pWaveInfo->workItemId[i], workItem))
            {
                isWiFound = true;
                break;
            }
        }
    }

    return (isWgFound && isWiFound);
}



void ExtractSymbolListFromELFBinary(const void* pBinary,
                                    size_t binarySize,
                                    std::vector<std::pair<std::string, uint64_t>>& outputSymbols)
{
    if ((nullptr == pBinary) || (0 == binarySize))
    {
        return;
    }

    // Determine the ELF type:
    bool isELF = false;
    bool isELF32 = false;
    bool isELF64 = false;

    // The ELF executable header is 16 bytes:
    if (16 < binarySize)
    {
        // Check for the ELF header start:
        const unsigned char* pBinaryAsUBytes = (const unsigned char*)pBinary;
        isELF = ((0x7f == pBinaryAsUBytes[0]) &&
                 ('E'  == pBinaryAsUBytes[1]) &&
                 ('L'  == pBinaryAsUBytes[2]) &&
                 ('F'  == pBinaryAsUBytes[3]));
        isELF32 = isELF && (0x01 == pBinaryAsUBytes[4]);
        isELF64 = isELF && (0x02 == pBinaryAsUBytes[4]);
    }

    // Validate:
    if (!isELF)
    {
        return;
    }

    if (!isELF32 && !isELF64)
    {
        assert(!"Unsupported ELF sub-format!");
        return;
    }

    // Set the version of elf:
    elf_version(EV_CURRENT);

    // Initialize the binary as ELF from memory:
    Elf* pContainerElf = elf_memory((char*)pBinary, binarySize);

    if (nullptr == pContainerElf)
    {
        return;
    }

    // First get the symbol table section:
    const void* pSymTab = nullptr;
    size_t symTabSize = 0;
    int symStrTabIndex = -1;
    static const std::string symTabSectionName = ".symtab";

    // Get the shared strings section:
    size_t sectionHeaderStringSectionIndex = 0;
    int rcShrstr = elf_getshdrstrndx(pContainerElf, &sectionHeaderStringSectionIndex);

    if ((0 != rcShrstr) || (0 == sectionHeaderStringSectionIndex))
    {
        return;
    }

    // Iterate the sections to find the symbol table:
    Elf_Scn* pCurrentSection = elf_nextscn(pContainerElf, nullptr);

    while (nullptr != pCurrentSection)
    {
        size_t strOffset = 0;
        size_t shLink = 0;

        if (isELF32)
        {
            // Get the section header:
            Elf32_Shdr* pCurrentSectionHeader = elf32_getshdr(pCurrentSection);

            if (nullptr != pCurrentSectionHeader)
            {
                // Get the name and link:
                strOffset = pCurrentSectionHeader->sh_name;
                shLink = pCurrentSectionHeader->sh_link;
            }
        }
        else if (isELF64)
        {
            // Get the section header:
            Elf64_Shdr* pCurrentSectionHeader = elf64_getshdr(pCurrentSection);

            if (nullptr != pCurrentSectionHeader)
            {
                // Get the name and link:
                strOffset = pCurrentSectionHeader->sh_name;
                shLink = pCurrentSectionHeader->sh_link;
            }
        }

        // Get the current section's name:
        char* pCurrentSectionName = elf_strptr(pContainerElf, sectionHeaderStringSectionIndex, strOffset);

        if (nullptr != pCurrentSectionName)
        {
            if (symTabSectionName == pCurrentSectionName)
            {
                // Get the section's data:
                Elf_Data* pSectionData = elf_getdata(pCurrentSection, nullptr);

                if (nullptr != pSectionData)
                {
                    // We got the the section info:
                    pSymTab = pSectionData->d_buf;
                    symTabSize = (size_t)pSectionData->d_size;      /*NOTE: WHy is this 0*/

                    // The linked section is the symbol table string section:
                    symStrTabIndex = (int)shLink;

                    // Found the section, no need to continue:
                    break;
                }
            }
        }

        // Get the next section:
        pCurrentSection = elf_nextscn(pContainerElf, pCurrentSection);
    }

    if (nullptr == pSymTab || 0 == symTabSize || (0 >= symStrTabIndex))
    {
        return;
    }

    if (isELF32)
    {
        const int numberOfSymbols = (int)(symTabSize / sizeof(Elf32_Sym));
        Elf32_Sym* pCurrentSymbol = (Elf32_Sym*)pSymTab;

        for (int i = 0; numberOfSymbols > i; ++i, ++pCurrentSymbol)
        {
            // Get the symbol name as a string:
            char* pCurrentSymbolName = elf_strptr(pContainerElf, symStrTabIndex, pCurrentSymbol->st_name);

            if (nullptr != pCurrentSymbolName)
            {
                // Add the symbol name to the list:
                std::pair<std::string, uint64_t> symbol;
                symbol = std::make_pair(pCurrentSymbolName, pCurrentSymbol->st_value);

                outputSymbols.push_back(symbol);
            }
        }
    }
    else if (isELF64)
    {
        const int numberOfSymbols = (int)(symTabSize / sizeof(Elf64_Sym));
        Elf64_Sym* pCurrentSymbol = (Elf64_Sym*)pSymTab;

        for (int i = 0; numberOfSymbols > i; ++i, ++pCurrentSymbol)
        {
            // Get the symbol name as a string:
            char* pCurrentSymbolName = elf_strptr(pContainerElf, symStrTabIndex, pCurrentSymbol->st_name);

            if (nullptr != pCurrentSymbolName)
            {
                // Add the symbol name to the list:
                std::pair<std::string, uint64_t> symbol;
                symbol = std::make_pair(pCurrentSymbolName, pCurrentSymbol->st_value);

                outputSymbols.push_back(symbol);
            }
        }
    }
}

#if 0
static std::stringstream gs_MsgStream;

// This counter will toggle between 0 and 1 based on how many times
// the DisassembleHsailLogFunction is called
static int gs_DissassembleCallCount = 0;

// \todo Need a way to get the kernel name
const static char* const gs_HSAIL_KERNEL_SYMBOL = "&__Gdt_vectoradd_kernel";

static void AgentHsailLogFunction(const char* msg, size_t size)
{
    HSAIL_UNREFERENCED_PARAMETER(size);

    if (gs_DissassembleCallCount == 0)
    {
        AgentLog("HSA-Agent Disassemble Call (0) will only print ISA\n");

        gs_DissassembleCallCount = gs_DissassembleCallCount + 1;

        return;
    }

    if (gs_DissassembleCallCount == 1)
    {
        AgentLog("HSA-Agent Disassemble Call (1) will print HSAIL text\n");

        gs_MsgStream << msg;
        // Reset the global counter
        gs_DissassembleCallCount = 0;
        return;
    }

}
#endif

#if 0
// This function's call has been commented out since we will be doing these steps in the
// agent to write the kernel to a file
static HsailAgentStatus DisassembleHsailTextFromAclBin(aclBinary* pBinary)
{
    acl_error errorCode = ACL_SUCCESS;
    aclCompiler* pCompiler = aclCompilerInit(nullptr, &errorCode);

    if (errorCode != ACL_SUCCESS)
    {
        assert(!"Error: aclCompilerInit failure");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    std::string strKernelName(gs_HSAIL_KERNEL_SYMBOL);

    // Clear up the strstream
    gs_MsgStream.str("");

    // There doesnt seem to be a need to change the kernel name like its done in the profiler
    std::string strKernelNameMangled = strKernelName;

    // The disassemble function needs to be called once, the callback is called twice
    errorCode = aclDisassemble(pCompiler, pBinary, strKernelNameMangled.c_str(), AgentHsailLogFunction);

    if (errorCode != ACL_SUCCESS)
    {
        assert(!"Error: aclDisassemble (1) failure");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    // Close the compiler handle
    errorCode = aclCompilerFini(pCompiler);

    if (errorCode != ACL_SUCCESS)
    {
        assert(!"Error: aclCompilerFini failure");
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    return HSAIL_AGENT_STATUS_SUCCESS;
}
#endif

// Code copied from HwDbgHSAContext.h. shows the location in ISA after the prolog code
static const void* GetLoadedCodeSegment(hsa_kernel_dispatch_packet_t* aql)
{
    amd_kernel_code_t* pKernelCode = reinterpret_cast<amd_kernel_code_t*>(aql->kernel_object);
    assert(nullptr != pKernelCode);

    if (nullptr == pKernelCode)
    {
        AGENT_ERROR("The kernel object in the aql packet is nullptr");
        return nullptr;
    }

    const amd_runtime_loader_debug_info_t* pKernelSymbol = reinterpret_cast<amd_runtime_loader_debug_info_t*>(pKernelCode->runtime_loader_kernel_symbol);
    assert(nullptr != pKernelSymbol);

    if (nullptr == pKernelSymbol)
    {
        AGENT_ERROR("The kernel object in aql contains a nullptr kernel symbol");
        return nullptr;
    }

    AGENT_LOG("The Loaded Code Segment is " << pKernelSymbol->owning_segment );
    return pKernelSymbol->owning_segment;
}

// This code is copied from the test utilities in HSAKernelUtils.cpp
static bool GetIsaBuffer(hsa_kernel_dispatch_packet_t* aql,
                         uint32_t**                    ppIsa,
                         uint32_t*                     pIsaLen,
                         uint64_t*                     isaByteOffsetOut )
{

    if (0 == aql->kernel_object)
    {
        AGENT_ERROR("Error in GetIsaBinary(): No kernel object found in AQL packet.");
        return false;
    }

    if (isaByteOffsetOut == nullptr)
    {
        return false;
    }

    bool ret = true;
    amd_kernel_code_t* pKernelCode = reinterpret_cast<amd_kernel_code_t*>(aql->kernel_object);
    *ppIsa = reinterpret_cast<uint32_t*>(aql->kernel_object + pKernelCode->kernel_code_entry_byte_offset);

    if (nullptr == *ppIsa)
    {
        ret = false;
        AGENT_ERROR("Error in GetIsaBinary(): cannot find ISA.");
        return ret;
    }
    else
    {
        AGENT_LOG("Loaded ISA location is " << *ppIsa);
    }

    const uint32_t S_END_PGM_INSTRUCTION = 0xbf810000;

    unsigned int i = 0;

    while ((*ppIsa)[i] != S_END_PGM_INSTRUCTION)
    {
        i++;
    }

    pIsaLen[0] = (i + 1);

    // The difference between the start of the ISA and the location after the prolog
    // code is the offset
    *isaByteOffsetOut = ((uint64_t) (*ppIsa)) - (uint64_t) GetLoadedCodeSegment(aql)  ;
    AGENT_LOG("The ISA offset (size of prolog code) is " << *isaByteOffsetOut << " Bytes" );


    return ret;
}

HsailAgentStatus AgentWriteISAToFile(const std::string&                  isaFileName,
                                           hsa_kernel_dispatch_packet_t* pAql)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    uint32_t* pIsa = nullptr;

    if (pAql == nullptr)
    {
        AGENT_ERROR("AgentWriteISAToFile: Invalid AQL packet");
        return status;
    }

    if (isaFileName.empty())
    {
        AGENT_ERROR("AgentWriteISAToFile: Invalid Filename");
        return status;
    }

    uint32_t isaLen;
    uint64_t isaOffset;
    bool success = GetIsaBuffer(pAql, &pIsa, &isaLen, &isaOffset);

    if (!success || pIsa == nullptr)
    {
        AGENT_ERROR("WriteISAToFile: Could not get ISA binary");
    }
    else
    {
        std::ofstream outstream;
        outstream.open(isaFileName.c_str()) ;

        if (outstream.is_open() == true)
        {
            unsigned int i = 0;

            while (i < isaLen)
            {
                outstream << std::dec << i << "\t"
                          << std::hex << "0x" << i << "\t"
                          << std::hex << "0x" << i * 4 << "\t"
                          << std::hex << "0x" << i * 4 + isaOffset << "\t"
                          << "0x" << pIsa[i] << "\n";
                i++;
            }

            outstream.close();
            AGENT_LOG("WriteISAToFile: ISA Length " << isaLen);

            status = HSAIL_AGENT_STATUS_SUCCESS;
        }
        else
        {
            AGENT_ERROR("AgentWriteISAToFile: Error writing to file");
            status = HSAIL_AGENT_STATUS_FAILURE;
        }
    }

    return status;
}
#if 0
HsailAgentStatus  AgentWriteHsailKernelToFile(const std::string kernelSource, const std::string filename)
{
    std::ofstream outfile;
    outfile.open(filename.c_str()) ;
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (outfile.is_open() == true)
    {
        outfile << kernelSource ;
        outfile << "\n";
        outfile.close();

        AGENT_OP("HSAIL kernel saved to \t " << filename);

        status = HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        AgentErrorLog("Could not write HSAIL kernel text to file\n");
        status = HSAIL_AGENT_STATUS_FAILURE;
    }

    return status;
}

HsailAgentStatus AgentDisassembleHsailKernel(void* pAclBinary, std::string& hsailKernelSourceOut)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (pAclBinary == nullptr)
    {
        AgentErrorLog("AgentDisassembleHsailKernel: aclBinary is nullptr\n");
        return status;
    }

    status = DisassembleHsailTextFromAclBin(reinterpret_cast<aclBinary*>(pAclBinary));

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AgentErrorLog("Could not disassemble the HSAIL kernel");
        hsailKernelSourceOut.assign("Invalid Disassembly");

        return status;
    }
    else
    {
        hsailKernelSourceOut.assign(gs_MsgStream.str().c_str());
    }

    return status;
}
#endif
