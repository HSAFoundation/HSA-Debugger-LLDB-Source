//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Class to manage the agent binary
//==============================================================================
#include <cassert>
// \todo including cstdint seems to need a -std=c++11 compiler switch on gcc
// Maye better to avoid C++11 for now.
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <iostream>
#include <unistd.h>

#include <hsa_ext_amd.h>
#include <amd_hsa_kernel_code.h>

#include <libelf.h>

#include "AgentBinary.h"
#include "AgentLogging.h"
#include "AgentNotifyGdb.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"
#include "CommunicationParams.h"
#include "AgentContext.h"

#include "AMDGPUDebug.h"

namespace HwDbgAgent
{


AgentBinary::~AgentBinary()
{

}

static uint32_t* ExtractIsaBinaryFromAQLPacket(const hsa_kernel_dispatch_packet_t* pAqlPacket)
{
    if (nullptr == pAqlPacket)
    {
        return nullptr;
    }

    amd_kernel_code_t* pKernelCode = reinterpret_cast<amd_kernel_code_t*>(pAqlPacket->kernel_object);
    return reinterpret_cast<uint32_t*>(pAqlPacket->kernel_object + pKernelCode->kernel_code_entry_byte_offset);
}

static uint32_t* ExtractIsaBinaryFromElfBinary(uint64_t    isaOffset,
                                               const void* pBinary,
                                               size_t      binarySize)
{
    if (nullptr == pBinary || 0 == binarySize)
    {
        return nullptr;
    }

    // get isa binary from .hsatext section with a relative byte offset (isaOffset)

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
        return nullptr;
    }

    if (!isELF32 && !isELF64)
    {
        assert(!"Unsupported ELF sub-format!");
        return nullptr;
    }

    // Set the version of elf:
    elf_version(EV_CURRENT);

    // Initialize the binary as ELF from memory:
    Elf* pContainerElf = elf_memory((char*)pBinary, binarySize);

    if (nullptr == pContainerElf)
    {
        return nullptr;
    }

    // First get the .hsatext section:
    static const std::string isaSectionName = ".hsatext";

    // Get the shared strings section:
    size_t sectionHeaderStringSectionIndex = 0;
    int rcShrstr = elf_getshdrstrndx(pContainerElf, &sectionHeaderStringSectionIndex);

    if ((0 != rcShrstr) || (0 == sectionHeaderStringSectionIndex))
    {
        return nullptr;
    }

    // Iterate the sections to find the isa section
    Elf_Scn* pCurrentSection = elf_nextscn(pContainerElf, nullptr);

    while (nullptr != pCurrentSection)
    {
        size_t strOffset = 0;

        if (isELF32)
        {
            // Get the section header:
            Elf32_Shdr* pCurrentSectionHeader = elf32_getshdr(pCurrentSection);

            if (nullptr != pCurrentSectionHeader)
            {
                // Get the name and link:
                strOffset = pCurrentSectionHeader->sh_name;
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
            }
        }

        // Get the current section's name:
        char* pCurrentSectionName = elf_strptr(pContainerElf, sectionHeaderStringSectionIndex, strOffset);

        if (nullptr != pCurrentSectionName)
        {
            if (isaSectionName == pCurrentSectionName)
            {
                // Get the section's data:
                Elf_Data* pSectionData = elf_getdata(pCurrentSection, nullptr);

                if (nullptr != pSectionData)
                {
                    // Found the section, no need to continue:
                    // the isa binary is prefixed with an amd_kernel_code_t structure
                    uint64_t buffer = reinterpret_cast<uint64_t>(pSectionData->d_buf) + isaOffset + sizeof(amd_kernel_code_t);
                    return reinterpret_cast<uint32_t*>(buffer);
                }
            }
        }

        // Get the next section:
        pCurrentSection = elf_nextscn(pContainerElf, pCurrentSection);
    }

    return nullptr;
}

static bool CanSkipInstructionCompare(const uint32_t firstInstruction, const uint32_t secondInstruction)
{
    /// Nop instruction
    const uint32_t S_NOP = 0xbf800000;

    /// Trap opcode, lower 8 bits specify trap_id
    const uint32_t S_TRAP_BASE = 0xbf920000;

    /// Mask to clear trapid of trap instruction
    const uint32_t S_TRAP_MASK = 0xffff0000;

    // if the two instruction are S_NOP or S_TRAP instruction, we can skip since the debugger patches the isa instruction in memory
    if ((firstInstruction == S_NOP  && ((secondInstruction & S_TRAP_MASK) == S_TRAP_BASE)) ||
        (secondInstruction == S_NOP && ((firstInstruction & S_TRAP_MASK)  == S_TRAP_BASE)))
    {
        return true;
    }

    return false;
}

static bool FindKernelNameUsingIsaComparison(const hsa_kernel_dispatch_packet_t* pAqlPacket,
                                             uint64_t                            isaOffset,
                                             const void*                         pBinary,
                                             size_t                              binarySize)
{
    const uint32_t* pIsaInAql = ExtractIsaBinaryFromAQLPacket(pAqlPacket);
    const uint32_t* pIsaInCodeObject = ExtractIsaBinaryFromElfBinary(isaOffset, pBinary, binarySize);

    if (nullptr == pIsaInAql || nullptr == pIsaInCodeObject)
    {
        AGENT_ERROR("FindKernelNameUsingIsaComparison: Invalid input parameters");

        return false;
    }

    /// \todo: the following is not efficient for a large isa
    // loop through the isa binary to check whether they are the same
    const uint32_t S_END_PGM_INSTRUCTION = 0xbf810000;
    uint64_t i = 0;

    while (pIsaInAql[i] == pIsaInCodeObject[i] || CanSkipInstructionCompare(pIsaInAql[i], pIsaInCodeObject[i]))
    {
        if (pIsaInAql[i] == S_END_PGM_INSTRUCTION)
        {
            // both isa binaries are exactly the same until the end of the program
            return true;
        }

        ++i;
    }

    AGENT_LOG("FindKernelNameUsingIsaComparison: Returned false");
    // the two isa binaries are different
    return false;
}

// \todo move stuff from agentutils to here
// The code to get the kernel name seems to get the debug symbol too, so stuff could be shared here
bool AgentBinary::PopulateKernelNameFromBinary(const hsa_kernel_dispatch_packet_t* pAqlPacket)
{
    if ((nullptr == m_pBinary) || (0 == m_binarySize))
    {
        return false;
    }

    std::string outputKernelName;

    // Get the symbol list (of pair of symbol string name and symbol value representing byte offset)
    std::vector<std::pair<std::string, uint64_t>> elfSymbols;
    ExtractSymbolListFromELFBinary(m_pBinary, m_binarySize, elfSymbols);

    // No symbols = nothing found:
    if (elfSymbols.empty())
    {
        // Can happen because of incorrect binaries or mismatched libelf header and libraries
        AGENT_ERROR("PopulateKernelNameFromBinary:Could not find elf symbols in DBE binary");
        return false;
    }

    // The matchable strings:
    static const std::string kernelNamePrefix1 = "&m::";   // kernel main function, the best match
    static const size_t kernelNamePrefix1Length = kernelNamePrefix1.length();   // Pre-calculate string lengths

    /// \todo: the following code is not robust (since the kernel name prefix may not always start with "&m::" or "&", we should check that the kernel symbol is STT_AMDGPU_HSA_KERNEL instead
    // Iterate the symbols to look for matches:
    const size_t symbolsCount = elfSymbols.size();
    bool foundMatch2 = false;

    for (size_t i = 0; symbolsCount > i; ++i)
    {
        const std::string& curSym = elfSymbols[i].first;
        AGENT_LOG("PopulateKernelNameFromBinary: Look for symbol " << curSym );

        if (    0 == curSym.compare(0, kernelNamePrefix1Length, kernelNamePrefix1) &&
                (curSym.length() > (kernelNamePrefix1Length + 1)) &&
                ('&' == curSym[kernelNamePrefix1Length])
            )
        {
            if (FindKernelNameUsingIsaComparison(pAqlPacket, elfSymbols[i].second, m_pBinary, m_binarySize))
            {
                // Found a level 1 match (the first one). It overrides any other matches, so return it!
                outputKernelName = curSym.substr(kernelNamePrefix1Length);
                AGENT_LOG("PopulateKernelNameFromBinary: Found a L1 Match");
                break;
            }
        }
        else if (!foundMatch2) // Only take the first level 2 match - and keep looking for level 1 matches
        {
            if ((curSym.length() > 1) && ('&' == curSym[0]) &&
                FindKernelNameUsingIsaComparison(pAqlPacket, elfSymbols[i].second, m_pBinary, m_binarySize))
            {
                outputKernelName = curSym;
                foundMatch2 = true;
                AGENT_LOG("PopulateKernelNameFromBinary: Found a L2 Match");
            }
        }
    }

    if (outputKernelName.empty())
    {
        AGENT_LOG("PopulateKernelNameFromBinary:No valid kernel name found");
        return false;
    }
    else
    {
        AGENT_LOG("PopulateKernelNameFromBinary: Kernel Name found " << outputKernelName);
    }

    m_kernelName = outputKernelName;

    return true;
}

void AgentBinary::PopulateWorkgroupSizeInformation(AgentContext* pAgentContext)
{
    if (pAgentContext != nullptr)
    {
        m_workGroupSize.x = pAgentContext->m_workGroupSize.x;
        m_workGroupSize.y = pAgentContext->m_workGroupSize.y;
        m_workGroupSize.z = pAgentContext->m_workGroupSize.z;
        m_gridSize.x = pAgentContext->m_gridSize.x;
        m_gridSize.y = pAgentContext->m_gridSize.y;
        m_gridSize.z = pAgentContext->m_gridSize.z;

        AGENT_LOG("Dispatch Dimensions WG:"
                  << m_workGroupSize.x << "x" << m_workGroupSize.y << "x" << m_workGroupSize.z
                  << "\tGridSize "
                  << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z);
    }
}

// Read the binary buffer and get the HL and LL symbol names
bool AgentBinary::GetDebugSymbolsFromBinary()
{

    if ((nullptr == m_pBinary) || (0 == m_binarySize))
    {
        return false;
    }

    // Get the symbol list:
    std::vector<std::pair<std::string, uint64_t>> elfSymbols;
    ExtractSymbolListFromELFBinary(m_pBinary, m_binarySize, elfSymbols);

    // No symbols = nothing found:
    if (elfSymbols.empty())
    {
        return false;
    }

    bool isllSymbolFound = false;

    // The matchable string
    static const std::string kernelNamePrefix1 = "__debug_isa__";   // ISA DWARF symbol
    static const size_t kernelNamePrefix1Length = kernelNamePrefix1.length();   // Pre-calculate string lengths

    // Iterate the symbols to look for matches:
    const size_t symbolsCount = elfSymbols.size();

    for (size_t i = 0; symbolsCount > i; ++i)
    {
        const std::string& curSym = elfSymbols[i].first;

        if (0 == curSym.compare(0, kernelNamePrefix1Length, kernelNamePrefix1))
        {
            // Found a level 1 match (the first one). It overrides any other matches, so return it!
            m_llSymbolName.assign(curSym);
            isllSymbolFound = true;
            break;
        }
    }

    // The HL symbol is always the same
    m_hlSymbolName.assign("__debug_brig__");

    if (isllSymbolFound)
    {
        return true;
    }

    AGENT_ERROR("GetDebugSymbolsFromBinary:Could not HL and LL symbols correctly");

    return false;


}

// Call the DBE and set up the buffer
HsailAgentStatus AgentBinary::PopulateBinaryFromDBE(HwDbgContextHandle dbgContextHandle,
                                                    const hsa_kernel_dispatch_packet_t* pAqlPacket)
{
    AGENT_LOG("Initialize a new binary");
    assert(dbgContextHandle != nullptr);

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (nullptr == dbgContextHandle)
    {
        AGENT_ERROR("Invalid DBE Context handle");
        return status;
    }

    // Note: Even though the DBE only gets a pointer for the binary,
    // the size of the binary is generated by the HwDbgHSAContext
    // by using ACL

    // Get the debugged kernel binary from DBE
    // A pointer to constant data
    HwDbgStatus dbeStatus = HwDbgGetKernelBinary(dbgContextHandle,
                                                 &m_pBinary,
                                                 &m_binarySize);

    assert(dbeStatus == HWDBG_STATUS_SUCCESS);
    assert(m_pBinary != nullptr);

    if (dbeStatus != HWDBG_STATUS_SUCCESS)
    {
        AGENT_ERROR(GetDBEStatusString(dbeStatus) <<
                    "PopulateBinaryFromDBE: Error in HwDbgGeShaderBinary");

        // Something was wrong we should exit without writing the binary
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    if (!PopulateKernelNameFromBinary(pAqlPacket))
    {
        AGENT_ERROR("PopulateBinaryFromDBE: Could not get the name of the kernel");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }

    return status;
}

const std::string AgentBinary::GetKernelName()const
{
    return m_kernelName;
}

/// Validate parameters of the binary, write the binary to shmem
/// and let gdb know we have a new binary
HsailAgentStatus AgentBinary::NotifyGDB() const
{

    HsailAgentStatus status;

    // Check that kernel name is not empty
    if (m_kernelName.empty())
    {
        AGENT_LOG("NotifyGDB: Kernel name may not have not been populated");
    }

    // Call function in AgentNotify
    // Let gdb know we have a new binary
    status = WriteBinaryToSharedMem(g_DBEBINARY_SHMKEY);

    if (HSAIL_AGENT_STATUS_FAILURE == status)
    {
        AGENT_ERROR("NotifyGDB: Could not write binary to shared mem");
        return status;
    }

#if 0
    // Call function in AgentNotify
    // Let gdb know where to get the ISA
    status = WriteIsaToSharedMem();

    if (HSAIL_AGENT_STATUS_FAILURE == status)
    {
        AGENT_ERROR("NotifyGDB: Could not write ISA to shared mem");
        return status;
    }

#endif

    status = AgentNotifyNewBinary(m_binarySize,
                                  m_hlSymbolName, m_llSymbolName,
                                  m_kernelName,
                                  m_workGroupSize, m_gridSize);

    if (HSAIL_AGENT_STATUS_FAILURE == status)
    {
        AGENT_ERROR("NotifyGDB: Couldnt not notify gdb");
        return status;
    }

    return status;
}
#if 0
HsailAgentStatus AgentBinary::WriteIsaToSharedMem() const
{

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (m_pBinary == nullptr)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary is null");
        return status;
    }

    // Maximum size of the shared memory buffer (1MB) used during initialization
    // The max size is half since it is shared
    static const size_t s_MAX_SIZE = g_BINARY_BUFFER_MAXSIZE;

    if (m_binarySize <= 0)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary size is 0");
        return status;
    }

    if (m_binarySize > s_MAX_SIZE)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary is larger than the shared mem allocated");
        return status;
    }

    // The shared mem segment needs place for a size_t value and the binary
    if ((m_binarySize + sizeof(size_t)) > s_MAX_SIZE)
    {
        AGENT_ERROR("WriteBinaryToShmem: Binary size is too big");
        return status;
    }

    // Get the pointer to the shmem segment
    void* pShm = AgentMapSharedMemBuffer(g_ISASTREAM_SHMKEY, g_ISASTREAM_MAXSIZE);

    if (pShm == (int*) - 1)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error with AgentMapSharedMemBuffer");
        return status;
    }

    // Copy the binary
    memcpy(pShm, m_pBinary, m_binarySize);

    // Detach shared memory
    status = AgentUnMapSharedMemBuffer(pShm);
    return status;
}
#endif

HsailAgentStatus AgentBinary::WriteBinaryToSharedMem(const key_t shmKey) const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (m_pBinary == nullptr)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary is null");
        return status;
    }

    // Maximum size of the shared memory buffer (1MB) used during initialization
    // The max size is half since it is shared
    static const size_t s_MAX_SIZE = g_BINARY_BUFFER_MAXSIZE;

    if (m_binarySize <= 0)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary size is 0");
        return status;
    }

    if (m_binarySize > s_MAX_SIZE)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error Binary is larger than the shared mem allocated");
        return status;
    }

    // The shared mem segment needs place for a size_t value and the binary
    if ((m_binarySize + sizeof(size_t)) > s_MAX_SIZE)
    {
        AGENT_ERROR("WriteBinaryToShmem: Binary size is too big");
        return status;
    }

    // Get the pointer to the shmem segment
    void* pShm = AgentMapSharedMemBuffer(shmKey, g_BINARY_BUFFER_MAXSIZE);

    if (pShm == (int*) - 1)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error with AgentMapSharedMemBuffer");
        return status;
    }

    // Write the size first
    size_t* pShmSizeLocation = (size_t*)pShm;
    pShmSizeLocation[0] = m_binarySize;

    AGENT_LOG("Binary Size is " << pShmSizeLocation[0]);

    // Write the binary after the size_t info
    void* pShmBinaryLocation = (size_t*)pShm + 1;

    // Copy the binary
    memcpy(pShmBinaryLocation, m_pBinary, m_binarySize);

    //printf("Debug OP");
    //for(int i = 0; i<10;i++)
    //{
    //    printf("%d \t %d\n",i,*((int*)pShmBinaryLocation + i));
    //}

    // Detach shared memory
    status = AgentUnMapSharedMemBuffer(pShm);

    if (status != HSAIL_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("WriteBinaryToShmem: Error with AgentUnMapSharedMemBuffer");
        return status;
    }

    return status;
}

HsailAgentStatus AgentBinary::WriteBinaryToFile(const char* pFilename) const
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (m_pBinary == nullptr)
    {
        AGENT_ERROR("WriteBinaryToFile: Error Binary is null");
        return status;
    }

    if (m_binarySize <= 0)
    {
        AGENT_ERROR("WriteBinaryToFile: Error Binary size is invalid");
        return status ;
    }

    if (pFilename == nullptr)
    {
        AGENT_ERROR("WriteBinaryToFile: File name is null");
        return status;
    }

    FILE* pFd = fopen(pFilename, "wb");

    if (pFd == nullptr)
    {
        AGENT_ERROR("WriteBinaryToFile: Error opening file");
        return status;
    }

    size_t retSize = fwrite(m_pBinary, sizeof(char), m_binarySize, pFd);

    fclose(pFd);

    if (retSize != m_binarySize)
    {
        AGENT_ERROR("WriteBinaryToFile: Error writing to file");
        assert(!"WriteBinaryToFile: Error: fwrite failure.");
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
        AGENT_LOG("DBE Binary Saved to " << pFilename);
    }

    return status;
}
} // End Namespace HwDbgAgent
