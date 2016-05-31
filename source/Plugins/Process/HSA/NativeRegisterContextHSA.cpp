#include <fstream>
//===-- NativeRegisterContextHSA.cpp --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NativeRegisterContextHSA.h"

#define DECLARE_REGISTER_INFOS_HSA_STRUCT
#include "RegisterInfosHSA.h"
#undef DECLARE_REGISTER_INFOS_HSA_STRUCT

#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/RegisterValue.h"
#include "Plugins/LanguageRuntime/HSA/HSARuntime/NativeHSADebug.h"
#include "lldb/Host/common/NativeThreadProtocol.h"

using namespace lldb;
using namespace lldb_private;

namespace
{
    static const uint32_t 
    g_regs[] = { 0, 1, 2, LLDB_INVALID_REGNUM };

    static const RegisterSet
    g_reg_set_hsa = { "General Purpose Registers", "gpr", 3, g_regs };
}

NativeRegisterContextHSA*
NativeRegisterContextHSA::CreateNativeRegisterContextHSA(NativeThreadProtocol &native_thread,
                                                         uint32_t concrete_frame_idx,
                                                         NativeHSADebug& hsa_debug)
{
    return new NativeRegisterContextHSA(native_thread, concrete_frame_idx, hsa_debug);
}

NativeRegisterContextHSA::NativeRegisterContextHSA (NativeThreadProtocol &native_thread,
                                                    uint32_t concrete_frame_idx,
                                                    NativeHSADebug& hsa_debug) :
    NativeRegisterContext (native_thread, concrete_frame_idx),
    m_hsa_debug (hsa_debug)
{

}

uint32_t
NativeRegisterContextHSA::GetRegisterSetCount () const
{
    return 1;
}

uint32_t
NativeRegisterContextHSA::GetUserRegisterCount() const
{
    return 3;
}

uint32_t
NativeRegisterContextHSA::GetRegisterCount() const
{
    return 3;
}


const RegisterSet *
NativeRegisterContextHSA::GetRegisterSet (uint32_t set_index) const
{
    return &g_reg_set_hsa;
}

Error
NativeRegisterContextHSA::ReadRegister (const RegisterInfo *reg_info, RegisterValue &reg_value)
{
    reg_value = (uint64_t)m_hsa_debug.GetPC(m_thread.GetID()-1);
    return Error();
}

Error
NativeRegisterContextHSA::WriteRegister (const RegisterInfo *reg_info, const RegisterValue &reg_value)
{
    return Error ();
}

Error
NativeRegisterContextHSA::ReadAllRegisterValues (lldb::DataBufferSP &data_sp)
{
    uint64_t data[] = { 0 };
    for (int i=0; i<3; ++i) data[i] = m_hsa_debug.GetPC(m_thread.GetID()-1);
    
    data_sp.reset(new DataBufferHeap(data, sizeof(data)));
    return Error();
}

Error
NativeRegisterContextHSA::WriteAllRegisterValues (const lldb::DataBufferSP &data_sp)
{
    Error error;

    return error;
}

const RegisterInfo *
NativeRegisterContextHSA::GetRegisterInfoAtIndex (uint32_t reg) const
{
    return &g_register_infos_hsa[reg];
}
