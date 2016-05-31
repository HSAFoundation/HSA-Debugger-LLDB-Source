//===-- NativeHSADebug.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_NativeHSADebug_h_
#define liblldb_NativeHSADebug_h_

#include <limits.h>
// C Includes
// C++ Includes
#include <algorithm>
#include <unordered_map>
#include "lldb/Core/Error.h"
#include "lldb/Host/File.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/Stream.h"
#include "lldb/Host/common/NativeProcessProtocol.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Host/ThreadLauncher.h"
// Other libraries and framework includes
#include "llvm/Support/FileSystem.h"
// Project includes
#include "HsaDebugPacket.h"
#include "CommunicationParams.h"
#include "Plugins/SymbolFile/AMDHSA/FacilitiesInterface.h"

namespace lldb_private {
    class HsaWavefront {
    public:
        HsaWavefront (const HsailAgentWaveInfo& info, const HsailWaveDim3& group_size) 
            : m_global_id (0),
              m_wave_info (info)
        {
            const auto& wg = info.workGroupId;
            m_global_id = wg.x + wg.y * group_size.x + wg.z * group_size.x * group_size.y;
        }

        uint64_t GetGlobalID() const { return m_global_id; }
        HsailWaveDim3 GetWorkGroupID() const { return m_wave_info.workGroupId; }
        const HsailWaveDim3* GetWorkItemIDs() const { return m_wave_info.workItemId; }
        uint64_t GetExecMask() const { return m_wave_info.execMask; }
        HsailWaveAddress GetWaveAddress() const { return m_wave_info.waveAddress; }
        HsailProgramCounter GetPC() const { return m_wave_info.pc; }

    private:
        uint64_t m_global_id;
        HsailAgentWaveInfo m_wave_info;
        
    };

    using HsaWavefronts = std::set<HsaWavefront>;

    inline bool operator< (const HsaWavefront& rhs, const HsaWavefront& lhs) {
        return rhs.GetGlobalID() < lhs.GetGlobalID();
    }

    void handleSigAlrm (int sig);

    class NativeHSADebug {
    private:
        void FlushPacketBuffer();
        void DispatchPacket (const HsaPacket& packet);
    
    public:
        NativeHSADebug(NativeProcessProtocol& native_process)
            : m_native_process(native_process),
              m_has_new_binary(false)
        {
            lldb_private::Error error;
            lldb_private::ThreadLauncher::LaunchThread("NativeHSADebug",
                                                       Run, this,
                                                       &error);
        }

        HsaWavefronts GetNewWavefronts() {
            Mutex::Locker locker (m_wavefront_mutex);

            HsaWavefronts ret;
            std::swap(m_new_wavefronts, ret);
            return ret;
        }

        const HsaWavefronts& GetWavefrontInfo() {
            Mutex::Locker locker (m_wavefront_mutex);
            return m_wavefront_info;
        }

        void SetBreakpoint(HwDbgInfo_addr addr);
        void DeleteBreakpoint(HwDbgInfo_addr addr);
        void EnableBreakpoint(HwDbgInfo_addr addr);
        void DisableBreakpoint(HwDbgInfo_addr addr);

        void SetMomentaryBreakpoint(HwDbgInfo_addr addrs);
        void KillAllWaves();
        void Continue();
        void DebuggingBegun(const HsaDebugNotificationPacket& packet);
        void DebuggingEnded(const HsaDebugNotificationPacket& packet);
        void NewBinary(const HsaDebugNotificationPacket& packet);
        void FocusChanged(const HsaDebugNotificationPacket& packet);

        bool HasNewBinary();

        bool IsKernelFinished() { return m_kernel_state == KernelState::Ended; }

        HwDbgInfo_addr GetPC(size_t wavefront_idx);

        static void* Run(void*);
        void DoRun();

        struct Registers {
            HwDbgInfo_addr pc;
            HwDbgInfo_addr fp;
            HwDbgInfo_addr mp;
        };
        Registers ReadRegisters (uint32_t frame_idx);

        bool JustHitBreakpoint(size_t wavefront_idx);

        std::string GetBinaryFileName() {
            return m_binary_file;
        }

    private:
        enum class KernelState {
            NotStarted, Started, Ended
                };

        void BreakpointHit(const HsaDebugNotificationPacket& packet);
        void StartDebugThread(const HsaDebugNotificationPacket& packet);
        void Predispatch(const HsaDebugNotificationPacket& packet);

        void UpdateWavefrontInfo  (std::size_t num_waves, HsailWaveDim3 work_group_size);

        void DispatchMomentaryBreakpoints();

        HsaDebugComms m_comms;
        bool m_debugging_begun = false;
        std::vector<std::unique_ptr<HsaPacket>> m_packet_buffer;
        KernelState m_kernel_state = KernelState::NotStarted;

        Mutex m_wavefront_mutex;
        HsaWavefronts m_wavefront_info;
        HsaWavefronts m_new_wavefronts;
        HsailWaveDim3 m_n_work_groups;
        HsailWaveDim3 m_work_items;
        HsailWaveDim3 m_work_group_size;
        bool m_is_stepping = false;

        std::vector<HwDbgInfo_addr> m_momentary_breakpoints;

        NativeProcessProtocol& m_native_process;
        
        std::string m_binary_file;
        bool m_has_new_binary;
    };

    using NativeHSADebugUP = std::unique_ptr<NativeHSADebug>;
} // namespace lldb_private

#endif // liblldb_NativeHSADebug_h_

