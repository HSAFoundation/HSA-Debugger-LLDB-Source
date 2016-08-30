//===-- GDBRemoteRegisterContextHSA.cpp -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "GDBRemoteRegisterContextHSA.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/Utils.h"
// Project includes
#include "Utility/StringExtractorGDBRemote.h"
#include "ProcessGDBRemote.h"
#include "ProcessGDBRemoteLog.h"
#include "ThreadGDBRemote.h"
#include "ThreadGDBRemoteHSA.h"
#include "Utility/ARM_DWARF_Registers.h"
#include "Utility/ARM_ehframe_Registers.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

GDBRemoteRegisterContextHSA::GDBRemoteRegisterContextHSA
(
    ThreadGDBRemote &thread,
    uint32_t concrete_frame_idx,
    GDBRemoteDynamicRegisterInfo &reg_info,
    bool read_all_at_once
) :
    GDBRemoteRegisterContext (thread, concrete_frame_idx, reg_info, read_all_at_once)
{}

GDBRemoteRegisterContextHSA::~GDBRemoteRegisterContextHSA()
{
}

bool
GDBRemoteRegisterContextHSA::ReadRegister (const RegisterInfo *reg_info, RegisterValue &value) 
{
    if (!reg_info) 
        return false;

    if (reg_info->kinds[eRegisterKindGeneric] == LLDB_REGNUM_GENERIC_PC) {
        return GDBRemoteRegisterContext::ReadRegister(reg_info, value);
    }

    auto unwinder = static_cast<ThreadGDBRemoteHSA&>(m_thread).GetUnwinder();
    if (!unwinder)
        return false;

    auto frame_sp = m_thread.GetFrameWithConcreteFrameIndex (m_concrete_frame_idx);
    auto reg_ctx_sp = unwinder->CreateRegisterContextForFrame(frame_sp.get());
    if (!reg_ctx_sp)
        return false;

    return reg_ctx_sp->ReadRegister(reg_info, value);
}

bool
GDBRemoteRegisterContextHSA::ReadAllRegisterValues (lldb::DataBufferSP &data_sp) {
    auto success = GDBRemoteRegisterContext::ReadAllRegisterValues(data_sp);
    
    if (!success)
        return false;

    auto bytes = data_sp->GetBytes();
    auto fp = GetFP(0);
    char* fp_p = reinterpret_cast<char*>(&fp);
    std::copy(fp_p, fp_p+4, bytes+8);

    return true;
}
