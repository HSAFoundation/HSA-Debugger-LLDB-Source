//===-- HsaDebugPacket.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_HsaDebugPacket_h_
#define liblldb_HsaDebugPacket_h_

#include <cstring>
#include <string>
#include <cstdio>
#include <memory>
#include "CommunicationControl.h"

class HsaPacket {
public:
  HsaPacket();
  virtual ~HsaPacket()=default;
  void dispatch (int stream) const;

  bool isCreateBreakpoint() const { return m_packet.m_command == HSAIL_COMMAND_CREATE_BREAKPOINT; }

protected:
  HsailCommandPacket m_packet;
};

class HsaCreateBreakpointPacket : public HsaPacket{
public:
  HsaCreateBreakpointPacket (int pc, int bkpt_id, std::string src_line, int line_num) {
    m_packet.m_command = HSAIL_COMMAND_CREATE_BREAKPOINT;
    m_packet.m_gdbBreakpointID = bkpt_id;
    m_packet.m_pc = static_cast<uint64_t>(pc);
    m_packet.m_lineNum = line_num;

    strncpy(m_packet.m_sourceLine, src_line.c_str(), AGENT_MAX_SOURCE_LINE_LEN);
  }
};

class HsaContinueDispatchPacket : public HsaPacket {
public:
  HsaContinueDispatchPacket () {
    m_packet.m_command = HSAIL_COMMAND_CONTINUE;
  }
};


class HsaCreateKernelBreakpointPacket : public HsaPacket {
public:
  HsaCreateKernelBreakpointPacket (std::string kernel_name, int bkpt_id) {
    m_packet.m_command = HSAIL_COMMAND_CREATE_BREAKPOINT;
    m_packet.m_gdbBreakpointID = bkpt_id;

    strncpy(m_packet.m_kernelName, kernel_name.c_str(), AGENT_MAX_FUNC_NAME_LEN);
  }
};

class HsaDeleteBreakpointPacket : public HsaPacket {
public:
  HsaDeleteBreakpointPacket (int bkpt_id) {
    m_packet.m_command = HSAIL_COMMAND_DELETE_BREAKPOINT;
    m_packet.m_gdbBreakpointID = bkpt_id ;
  }
};

class HsaEnableBreakpointPacket : public HsaPacket {
public:
  HsaEnableBreakpointPacket (int bkpt_id) {
    m_packet.m_command = HSAIL_COMMAND_ENABLE_BREAKPOINT;
    m_packet.m_gdbBreakpointID = bkpt_id ;
  }
};


class HsaDisableBreakpointPacket : public HsaPacket {
public:
  HsaDisableBreakpointPacket (int bkpt_id) {
    m_packet.m_command = HSAIL_COMMAND_DISABLE_BREAKPOINT;
    m_packet.m_gdbBreakpointID = bkpt_id ;
  }
};

class HsaMomentaryBreakpointPacket : public HsaPacket {
public:
  HsaMomentaryBreakpointPacket (int n_bkpts) {
    m_packet.m_command = HSAIL_COMMAND_MOMENTARY_BREAKPOINT;
    m_packet.m_numMomentaryBP = n_bkpts;
  }
};

class HsaSetLoggingPacket : public HsaPacket {
public:
  HsaSetLoggingPacket (HsailLogCommand logging_command) {
    m_packet.m_command = HSAIL_COMMAND_SET_LOGGING;
    m_packet.m_loggingInfo = logging_command;
  }
};

class HsaKillPacket : public HsaPacket {
public:
  HsaKillPacket () {
    m_packet.m_command = HSAIL_COMMAND_KILL_ALL_WAVES;
  }
};


class HsaDebugNotificationPacket {
public:
  HsaDebugNotificationPacket(HsailNotificationPayload packet) : m_packet(packet)
  { 
  }

  static std::unique_ptr<HsaDebugNotificationPacket> readFromFd (int fd);

  //private:
  HsailNotificationPayload m_packet;
};

class HsaDebugComms {
public:
  HsaDebugComms();
  void init();
  void dispatchPacket (const HsaPacket& packet);
  std::unique_ptr<HsaDebugNotificationPacket> readPacket ();

private:
  int read_fd;
  int write_fd;
};


#endif // liblldb_HsaDebugPacket
