//===-- GDBRemoteRegisterContext⎈.h ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_GDBRemoteRegisterContextHSA_h_
#define lldb_GDBRemoteRegisterContextHSA_h_

// C Includes
// C++ Includes
#include <vector>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Target/RegisterContext.h"
#include "Plugins/Process/Utility/DynamicRegisterInfo.h"

#include "GDBRemoteCommunicationClient.h"
#include "GDBRemoteRegisterContext.h"

class StringExtractor;

namespace lldb_private {
namespace process_gdb_remote {

class ThreadGDBRemote;
class ProcessGDBRemote;

class GDBRemoteRegisterContextHSA : public GDBRemoteRegisterContext
{
public:
    GDBRemoteRegisterContextHSA (ThreadGDBRemote &thread,
                                 uint32_t concrete_frame_idx,
                                 GDBRemoteDynamicRegisterInfo &reg_info,
                                 bool read_all_at_once);

    ~GDBRemoteRegisterContextHSA() override;

    bool
    ReadRegister (const RegisterInfo *reg_info, RegisterValue &value) override;

    bool
    ReadAllRegisterValues (lldb::DataBufferSP &data_sp) override;

private:
    DISALLOW_COPY_AND_ASSIGN (GDBRemoteRegisterContextHSA);
};

} // namespace process_gdb_remote
} // namespace lldb_private

#endif // lldb_GDBRemoteRegisterContextHSA_h_
