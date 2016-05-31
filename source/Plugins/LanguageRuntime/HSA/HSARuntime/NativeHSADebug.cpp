#include "NativeHSADebug.h"
#include <cstdlib>
#include <memory>
#include <sys/mman.h>

#include "HsaUtils.h"

using namespace lldb_private;


template <typename T>
struct shmem_delete {
    void operator() (T* t) {
        shmdt(t);
    }
};

template <typename T>
using shmem_ptr = std::unique_ptr<T, shmem_delete<T>>;

template <typename T>
shmem_ptr<T> SharedMemGet (int key, std::size_t max_size) {
    int shmid = shmget(key, max_size, 0666);

    if (shmid < 0) {
        return nullptr;
    }

    auto pShm = shmat(shmid, NULL, 0);
    if (pShm == NULL || pShm == ((void*)-1)) {
        return nullptr;
    }

    return shmem_ptr<T>{static_cast<T*>(pShm)};
}

shmem_ptr<HsailAgentWaveInfo> GetWaveInfoMem() {
    return SharedMemGet<HsailAgentWaveInfo>(g_WAVE_BUFFER_SHMKEY, g_WAVE_BUFFER_MAXSIZE);
}

shmem_ptr<HsailMomentaryBP> GetMomentaryBpMem() {
    return SharedMemGet<HsailMomentaryBP>(g_MOMENTARY_BP_BUFFER_SHMKEY, g_MOMENTARY_BP_BUFFER_MAXSIZE);
}

shmem_ptr<char> GetBinaryMem() {
    return SharedMemGet<char>(g_DBEBINARY_SHMKEY, g_BINARY_BUFFER_MAXSIZE);
}

void NativeHSADebug::FlushPacketBuffer() {
    for (auto& packet : m_packet_buffer) {
        LogMsg("HSARuntime::FlushPacketBuffer: Dispatching");
        m_comms.dispatchPacket(*packet);
    }
    m_packet_buffer.clear();
}

void NativeHSADebug::DispatchPacket (const HsaPacket& packet) {
    if (m_debugging_begun) {
        m_comms.dispatchPacket(packet);
        LogMsg("HSARuntime::DispatchPacket: Dispatching");
    } else {
        LogMsg("HSARuntime::DispatchPacket: Buffering");
        m_packet_buffer.emplace_back(new HsaPacket(packet));
    }
}

void NativeHSADebug::SetMomentaryBreakpoint(HwDbgInfo_addr addr) {
    LogBkpt("NativeHSADebug::SetMomentaryBreakpoint at pc %d", addr);
    m_momentary_breakpoints.push_back(addr);
}

void NativeHSADebug::DispatchMomentaryBreakpoints() {
    LogBkpt("NativeHSADebug::DispatchMomentaryBreakpoints");
    auto momentary_bp_up = GetMomentaryBpMem();
    auto momentary_bp = momentary_bp_up.get();

    for (size_t i=0; i < m_momentary_breakpoints.size(); ++i) {
        momentary_bp[i].m_pc = m_momentary_breakpoints[i];
    }

    HsaMomentaryBreakpointPacket packet (m_momentary_breakpoints.size());
    DispatchPacket(packet);
    m_momentary_breakpoints.clear();
}

void NativeHSADebug::DeleteBreakpoint(HwDbgInfo_addr addr) {
    HsaDeleteBreakpointPacket packet (0);
    DispatchPacket(packet);
}

void NativeHSADebug::EnableBreakpoint(HwDbgInfo_addr addr) {
    HsaEnableBreakpointPacket packet (0);
    DispatchPacket(packet);
}

void NativeHSADebug::DisableBreakpoint(HwDbgInfo_addr addr) {
    HsaDisableBreakpointPacket packet (0);
    DispatchPacket(packet);
}

void NativeHSADebug::SetBreakpoint(HwDbgInfo_addr addr) {
    auto line = 0;
    HsaCreateBreakpointPacket packet (addr, 0, "", line);
    DispatchPacket(packet);
}

void NativeHSADebug::KillAllWaves() {
    HsaKillPacket packet;
    DispatchPacket(packet);
}

void NativeHSADebug::Continue() {
    if (!m_momentary_breakpoints.empty()) {
        DispatchMomentaryBreakpoints();
    }

    HsaContinueDispatchPacket packet;
    DispatchPacket(packet);
}


void NativeHSADebug::DebuggingBegun(const HsaDebugNotificationPacket& packet) {
    m_debugging_begun = true;
    m_kernel_state = KernelState::Started;
  
    FlushPacketBuffer();
}

void NativeHSADebug::DebuggingEnded(const HsaDebugNotificationPacket& packet) {
    m_kernel_state = KernelState::Ended;
}


void NativeHSADebug::Predispatch(const HsaDebugNotificationPacket& packet) {
}

void NativeHSADebug::StartDebugThread(const HsaDebugNotificationPacket& packet) {
}

void NativeHSADebug::NewBinary(const HsaDebugNotificationPacket& packet) {
    m_has_new_binary = true;

    m_n_work_groups = packet.m_packet.payload.BinaryNotification.m_workGroupSize;
    m_work_items = packet.m_packet.payload.BinaryNotification.m_gridSize;
    m_work_group_size.x = m_work_items.x == 0 || m_n_work_groups.x == 0 ? 0 : m_work_items.x / m_n_work_groups.x;
    m_work_group_size.y = m_work_items.y == 0 || m_n_work_groups.y == 0 ? 0 : m_work_items.y / m_n_work_groups.y;
    m_work_group_size.z = m_work_items.z == 0 || m_n_work_groups.z == 0 ? 0 : m_work_items.z / m_n_work_groups.z;

    //signal as soon as possible to avoid race conditions
    m_native_process.Signal(SIGCHLD);
}

bool NativeHSADebug::HasNewBinary() {
    if (!m_has_new_binary) return false;

    auto raw_bin_up = GetBinaryMem();
    auto raw_bin = raw_bin_up.get();

    std::size_t binary_size = reinterpret_cast<std::size_t*>(raw_bin)[0];
    std::vector<char> binary (binary_size);
    raw_bin += sizeof(std::size_t);
    std::copy(raw_bin, raw_bin + binary_size, std::begin(binary));

    llvm::SmallString<PATH_MAX> output_file_path{};
    int temp_fd;
    llvm::sys::fs::createTemporaryFile("hsa_binary.%%%%%%", "", temp_fd, output_file_path);
    File file (temp_fd, true);

    m_binary_file = output_file_path.c_str();

    size_t bytes_written = binary.size();
    if (file.Write(binary.data(), bytes_written).Success()) {

    }

    m_has_new_binary = false;
    
    return true;
}

void NativeHSADebug::FocusChanged(const HsaDebugNotificationPacket& packet) {

}

void NativeHSADebug::DoRun () {
    m_comms.init();
    while (true) {
        auto packet = m_comms.readPacket();

        if (packet) {
            LogMsg("NativeHSADebug: Got notification packet");
            switch (packet->m_packet.m_Notification) {
            case HSAIL_NOTIFY_BREAKPOINT_HIT: LogMsg("HSAIL_NOTIFY_BREAKPOINT_HIT"); BreakpointHit(*packet); break;
            case HSAIL_NOTIFY_NEW_BINARY: LogMsg("HSAIL_NOTIFY_NEW_BINARY"); NewBinary(*packet); break;
            case HSAIL_NOTIFY_AGENT_UNLOAD: LogMsg("HSAIL_NOTIFY_AGENT_UNLOAD"); break;
            case HSAIL_NOTIFY_BEGIN_DEBUGGING: LogMsg("HSAIL_NOTIFY_BEGIN_DEBUGGING"); DebuggingBegun(*packet); break;
            case HSAIL_NOTIFY_END_DEBUGGING: LogMsg("HSAIL_NOTIFY_END_DEBUGGING"); DebuggingEnded(*packet); break;
            case HSAIL_NOTIFY_FOCUS_CHANGE: LogMsg("HSAIL_NOTIFY_FOCUS_CHANGE"); FocusChanged(*packet); break;
            case HSAIL_NOTIFY_START_DEBUG_THREAD: LogMsg("HSAIL_NOTIFY_START_DEBUG_THREAD"); StartDebugThread(*packet); break; 
            case HSAIL_NOTIFY_PREDISPATCH_STATE: LogMsg("HSAIL_NOTIFY_PREDISPATCH_STATE"); Predispatch(*packet);break;
            case HSAIL_NOTIFY_UNKNOWN:
            default: break;
            }
        }
    }
}

void* NativeHSADebug::Run(void* void_debug) {
    auto debug = static_cast<NativeHSADebug*>(void_debug);
    debug->DoRun();
    return 0;
}


void NativeHSADebug::UpdateWavefrontInfo (std::size_t num_waves, HsailWaveDim3 work_group_size) {
    Mutex::Locker locker (m_wavefront_mutex);

    auto wave_info = GetWaveInfoMem();
    m_wavefront_info.clear();
    m_new_wavefronts.clear();
  
    if (m_wavefront_info.size() != num_waves) {
        for (size_t i=0; i < num_waves; ++i) {
            m_new_wavefronts.emplace(wave_info.get()[i], work_group_size);
        }
    }

    for (size_t i=0; i < num_waves; ++i) {
        m_wavefront_info.emplace(wave_info.get()[i], work_group_size);
    }
}

void NativeHSADebug::BreakpointHit(const HsaDebugNotificationPacket& packet) {
    LogMsg("HSARuntime::BreakpointHit");

    UpdateWavefrontInfo(packet.m_packet.payload.BreakpointHit.m_numActiveWaves, m_work_group_size);

    m_kernel_state = KernelState::Started;
}

NativeHSADebug::Registers NativeHSADebug::ReadRegisters (uint32_t frame_idx) {
    Registers reg;

    return reg;
}

HwDbgInfo_addr NativeHSADebug::GetPC (size_t wavefront_idx) {
    auto wave_info = GetWaveInfoMem();
    if (!wave_info) return 0;
    return wave_info.get()[wavefront_idx].pc;
}

bool NativeHSADebug::JustHitBreakpoint (size_t wavefront_idx) {
    return true;
}
