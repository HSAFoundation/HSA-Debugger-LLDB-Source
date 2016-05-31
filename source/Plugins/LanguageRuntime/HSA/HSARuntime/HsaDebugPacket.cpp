#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "CommunicationParams.h"
#include "CommunicationControl.h"

#include "HsaDebugPacket.h"
#include "HsaUtils.h"

using namespace lldb_private;

HsaPacket::HsaPacket () {
  memset((void*)&m_packet, 0, sizeof(HsailCommandPacket));
  m_packet.m_command = HSAIL_COMMAND_UNKNOWN;
  m_packet.m_loggingInfo = HSAIL_LOGGING_UNKNOWN;
  m_packet.m_hitCount = -1;
  m_packet.m_gdbBreakpointID = -1;
  m_packet.m_pc = (uint64_t)HSAIL_ISA_PC_UNKOWN;
  m_packet.m_lineNum = -1;
  m_packet.m_numMomentaryBP = 0;

  m_packet.m_conditionPacket.m_conditionCode = HSAIL_BREAKPOINT_CONDITION_UNKNOWN;
  m_packet.m_conditionPacket.m_workgroupID.x = -1;
  m_packet.m_conditionPacket.m_workgroupID.y = -1;
  m_packet.m_conditionPacket.m_workgroupID.z = -1;
  m_packet.m_conditionPacket.m_workitemID.x = -1;
  m_packet.m_conditionPacket.m_workitemID.y = -1;
    
  for (int i=0; i < AGENT_MAX_SOURCE_LINE_LEN; i++) {
    m_packet.m_sourceLine[i] = '\0';
  }

  for (int i=0; i < AGENT_MAX_FUNC_NAME_LEN; i++) {
    m_packet.m_kernelName[i] = '\0';
  }
}

void HsaPacket::dispatch (int fd) const {
  write(fd, reinterpret_cast<const void*>(&m_packet), sizeof(m_packet));
}

HsaDebugComms::HsaDebugComms () {

}

void HsaDebugComms::init() {
  mkfifo(gs_GdbToAgentFifoName, g_FIFO_PERMISSIONS);
  mkfifo(gs_AgentToGdbFifoName, g_FIFO_PERMISSIONS);

  read_fd = open(gs_AgentToGdbFifoName, O_RDONLY | O_NONBLOCK);
  write_fd = open(gs_GdbToAgentFifoName, O_WRONLY);
}

void HsaDebugComms::dispatchPacket (const HsaPacket& packet) {
  packet.dispatch(write_fd);
}

std::unique_ptr<HsaDebugNotificationPacket> HsaDebugComms::readPacket () {
  return HsaDebugNotificationPacket::readFromFd(read_fd);
}


std::unique_ptr<HsaDebugNotificationPacket> HsaDebugNotificationPacket::readFromFd (int fd) {
  HsailNotificationPayload packet;
  ssize_t n_read = read(fd, static_cast<void*>(&packet), sizeof(packet));

  if (n_read == 0 || n_read == -1) {
    return nullptr;
  }

  return std::unique_ptr<HsaDebugNotificationPacket>(new HsaDebugNotificationPacket(packet));
}
