//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief  HSA Runtime resource management class
//==============================================================================
#ifndef _HSA_RESOURCE_MANAGER_H_
#define _HSA_RESOURCE_MANAGER_H_

#include <cstddef>
#include <cstdint>  //  UINT64_MAX
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #pragma warning(push, 3)
#endif
#include <hsa.h>
#include <hsa_ext_amd.h>
#include <hsa_ext_finalize.h>
#if defined(_WIN32) || defined(_WIN64)
    #pragma warning(pop)
#endif

/// \brief Check status macro that also report file name and line number in application debug build.
///
/// \param[in] x The status need to be check
/// \return true if x == HSA_STATUS_SUCCESS
#if defined (_DEBUG) || defined (DEBUG)
    #define HSA_CHECK_STATUS( x ) DevTools::HsaCheckStatus(x, __FILE__, __LINE__)
#else
    #define HSA_CHECK_STATUS( x ) DevTools::HsaCheckStatus(x)
#endif

namespace DevTools
{
static const std::size_t gs_MAX_ARGS_BUFF_SIZE = 256;

/// \brief A struct holding HSA agent (Device) information
typedef struct AgentInfo
{
    // Agent device handle
    hsa_agent_t m_device;

    // Device chip ID
    uint32_t m_chipID;

    // Agent profile (FULL or BASE)
    hsa_profile_t m_profile;

    // Max size of Queue buffer
    uint32_t m_maxQueueSize;

    // Device local coarse grain memory region
    hsa_region_t coarseRegion;

    // Device local fine grain memory region
    hsa_region_t fineRegion;

    // Memory region supporting kernel arguments
    hsa_region_t kernargRegion;

    AgentInfo() : m_device({0}), m_chipID(0), m_maxQueueSize(0),
              coarseRegion({0}), fineRegion({0}), kernargRegion({0})
    {}
} AgentInfo;

class HSAResourceManager
{
public:
    /// \brief Default constructor, initialize member variable
    HSAResourceManager();

    /// \brief Destructor, will call Destroy() to release resources.
    ~HSAResourceManager();

    /// \brief Call hsa_init() and setup a default GPU device
    /// \param[in] verbosePrint  set to true to print extra message outputs to console
    ///
    /// \return true if there is no error
    static bool InitRuntime(bool verbosePrint = true);

    /// \brief DetroyQueue() and then shut down HSA runtime
    ///
    /// \return true if there is no error
    static bool ShutDown();

    /// \brief Print hsa version numbers
    ///
    /// \return true if no error
    static bool PrintHsaVersion();

    /// \brief Get HSA version numbers
    ///
    /// \param[out] major Major version number
    /// \param[out] minor Minor version number
    /// \return true if no error
    static bool GetHsaVersion(uint16_t& major, uint16_t& minor);

    /// \brief Create a default HSA GPU queue. Note: assuming there is a
    ///        single queue with a single program
    /// \param[in] enableKernelTimestamps flag indicating whether or not profiling is enabled on this queue
    /// \return true if there is no error
    static bool CreateDefaultQueue(bool enableKernelTimestamps = false);

    /// \brief overide the default queue with the specified queue (deleting the default queue if necessary)
    ///
    /// \param[in] pQueue the queue to replace the default queue with
    /// \return true if there is no error
    static bool SetQueue(hsa_queue_t* pQueue);

    /// \brief Finalize BRIG and create a default aql packet with 1 workitem
    ///
    /// \param[in]  brig            BRIG module to be finalize to ISA.
    /// \param[in]  kernelSymbol    Kernel entry point.
    /// \param[in]  bCreateSignal   Tell the function to whether to create
    ///                               a default signal value for AQL packet or not.
    /// \param[out] aqlPacketOut    The AQL packet which the finalized HSA program
    ///                             information will be put into.
    /// \param[in]  finalizerFlags  Additional compilation flags for finalizer.
    /// \return true if there is no error
    bool CreateAQLPacketFromBrig(
        const void*                   pBRIG,
        const std::string&            kernelSymbol,
        const bool                    bCreateSignal,
        hsa_kernel_dispatch_packet_t& aqlPacketOut,
        const std::string&            finalizerFlags = "");

    /// \brief Create a default aql packet from the existing executable with relative kernel symbol
    ///
    /// \param[in]  kernelSymbol    Kernel entry point
    /// \param[in]  bCreateSignal   Tell the function to whether to create
    ///                             a default signal value for AQL packet or not.
    /// \param[out] aqlPacketOut    The AQL packet which the finalized HSA program
    ///                             information will be put into.
    /// \return true if there is no error
    bool CreateAQLFromExecutable(
        const std::string&            kernelSymbol,
        const bool                    bCreateSignal,
        hsa_kernel_dispatch_packet_t& aqlPacketOut);

    /// \brief Copy one aql packet setting to another
    ///
    /// \param[in]  aqlPacket     The AQL packet to be copied from
    /// \param[in]  bCopySignal   Tell the function whether to copy the signal value from iAql.
    /// \param[out] aqlPacketOut  The AQL packet to be copied to.
    /// \return true if there is no error
    bool CopyKernelDispatchPacket(
        const hsa_kernel_dispatch_packet_t& aqlPacket,
        const bool                          bCopySignal,
        hsa_kernel_dispatch_packet_t& aqlPacketOut) const;

    /// \brief Append a kernel argument into argument buffer, m_argsBuff
    ///
    /// \param[in] pAddrToCopyIn The address of input kernel argument
    /// \param[in] argsSizeInBytes Size of the input kernel argument
    /// \param[in] offsetSize Depends on the source, we have additional global offset argument.
    ///                       For now the default value is for OpenCL 2.0 kernel.
    /// \return true if there is no error
    bool AppendKernelArgs(
        const void*       pAddrToCopyIn,
        const std::size_t argsSizeInBytes,
        const std::size_t offsetSize = sizeof(uint64_t) * 6);

    /// \brief Register kernel arguments buffer to the runtime
    ///
    /// \param[in] aqlPacket The aql packet which will take the kernel argument buffer
    /// \return true if there is no error
    bool RegisterKernelArgsBuffer(hsa_kernel_dispatch_packet_t& aqlPacket);

    /// \brief Deregister kernel arguments buffer.
    ///
    /// \return true if there is no error
    bool DeregisterKernelArgsBuffer();

    /// \brief Dispatch AQL kernel dispatch packet
    ///
    /// \param[in] aqlPacket The AQL packet going to be dispatch.
    /// \return true if there is no error
    bool Dispatch(hsa_kernel_dispatch_packet_t& aqlPacket);

    /// \brief Wait for the AQL packet completion signal value be set to 0 as completion.
    ///        Once the AQL dispatch complete, the signal value will be set back to 1 by
    ///        this function.
    ///
    /// \param[in] completionSignal The completion signal to be waited on
    /// \param[in] timeout          Maximum duration of the wait.  Specified in the same unit as the system timestamp.
    /// \param[in] outputTimingData flag indicating whether or not kernel timing info
    ///            is output to the console (only available if CreateDefaultQueue was
    ///            called with kernel timestamps enabled)
    /// \return true if there is no error
    bool WaitForCompletion(hsa_signal_t& completionSignal, uint64_t timeout = UINT64_MAX, bool outputTimingData = false);

    /// \brief Create a signal with default value 1.
    ///
    /// \param[out] signalOut The signal handle to be put into the created signal variable.
    /// \return true if there is no error
    bool CreateSignal(hsa_signal_t& signalOut);

    /// \brief Destroy a signal
    ///
    /// \param[in,out] signal The signal going to be destroy.
    /// \return true if there is no error
    bool DestroySignal(hsa_signal_t& signal);

    /// \brief Clean up function, release all signal variables which are created by CreateSignal(),
    ///        and m_prog, the HSA program handle member variable.
    ///
    /// \return true if there is no error
    bool CleanUp();

    /// \brief Destroy the queue created by CreateDefaultQueue().
    ///
    /// \return true if there is no error
    static bool DestroyQueue();

    /// \brief Allocate HSA device local memory in coarse grain region, if there is.
    ///
    /// \param[in] size Size of memory to be allocated, in bytes
    /// \return Pointer to the allocated memory location, NULL if fail.
    static void* AllocateCoarseLocalMemory(size_t size);

    /// \brief Allocate HSA kerenarg memory region
    ///
    /// \param[in] size Size of memory to be allocated, in bytes
    /// \return Pointer to the allocated memory location, NULL if fail.
    static void* AllocateSysMemory(size_t size);

    /// \brief Free HSA memory
    ///
    /// \param[in] Pointer to the memory location to be freed.
    /// \return true if there is no error.
    static bool FreeHSAMemory(void* pBuffer);

    /// \brief Copy HSA memory
    ///
    /// \param[in] pDest Pointer to destination memory location.
    /// \param[in] pSrc Pointer to source memory location.
    /// \param[in] size Size of memory to be copy, in bytes.
    /// \param[in] hostToDev Marker to tell whether it is going to be copied from host (CPU) to device (GPU).
    /// \return true if there is no error.
    static bool CopyHSAMemory(void* pDest, const void* pSrc, std::size_t size, bool hostToDev);


    /// \brief return whether it has an HSA runtime initialized in it.
    ///
    /// \return true if there is already an HSA runtime.
    static bool HasRuntime();

    // Accessors
    /// \brief return GPU agent info
    ///
    /// \return GPU agent info
    static const AgentInfo& GPUInfo();

    /// \brief return CPU agent info
    ///
    /// \return CPU agent info
    static const AgentInfo& CPUInfo();

    /// \brief return the GPU agent device
    ///
    /// \return GPU agent device
    static const hsa_agent_t& GPU();

    // Accessors
    /// \brief return the CPU agent device
    ///
    /// \return GPU agent device
    static const hsa_agent_t& CPU();

    /// \brief Query GPU chip ID
    ///
    /// \return GPU chip ID
    static const uint32_t& GPUChipID();

    /// \brief Query CPU chip ID
    ///
    /// \return CPU chip ID
    static const uint32_t& CPUChipID();

    /// \brief return the default queue
    ///
    /// \return the default queue
    static hsa_queue_t* const& Queue();

private:
    /// \brief Copy constructor, temporary banned
    HSAResourceManager(const HSAResourceManager&);

    /// \brief assignment operator, temporary banned
    void operator=(const HSAResourceManager&);

    /// \brief Finalize pBRIG to m_executable
    bool Finalize(
        const void*                 pBRIG,
        const std::string&          compileFlags);

    // Member variables
    static uint16_t ms_hsaCount;
    static bool ms_hasRuntime;
    static bool ms_profilingEnabled;

    /// \todo: Inorder to support multi-GPU system we should use vector<>
    //        (or something similar) here.
    static AgentInfo   ms_gpu;
    static AgentInfo   ms_cpu;

    static hsa_queue_t* ms_pQueue;

    hsa_executable_t m_executable;
    hsa_code_object_t m_codeObj;

    std::vector<hsa_signal_t> m_signals;

    unsigned char* m_pArgsBuff;
    std::size_t m_maxArgSize;
    std::size_t m_argsSize;
};

/// \brief Check status.
///
/// \param[in] s The status need to be check
/// \return true if s == HSA_STATUS_SUCCESS
bool HsaCheckStatus(hsa_status_t s);

/// \brief Check status.
///
/// \param[in] s The status need to be check
/// \param[in] fileName Current file name
/// \param[in] lineNum Current line number in file
/// \return true if s == HSA_STATUS_SUCCESS
bool HsaCheckStatus(hsa_status_t s, const std::string& fileName, int lineNum);

/// \brief Convert hsa status enum to string
///
/// \param[in] s  the hsa status enum
/// \return return the string
std::string HsaStatusStrings(hsa_status_t s);

/// \brief Enable or disable soft CP mode (set HSA_EMULATE_AQL and HSA_TOOLS_LIB environment variables)
///
/// \param[in] bEnable       true to enable soft CP mode, false to disable soft CP mode
/// \param[in] verbosePrint  set to true to print the extra message outputs to console
/// \return true if successful
bool SetSoftCPMode(bool bEnable, bool verbosePrint = true);

} // namespace DevTools

#endif // _HSA_RESOURCE_MANAGER_H_
