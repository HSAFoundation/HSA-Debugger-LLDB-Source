//===-- RegisterContextHSA.cpp ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//

#include <stddef.h>
#include <vector>
#include <cassert>

#include "llvm/Support/Compiler.h"
#include "lldb/Target/Thread.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/lldb-defines.h"

#include "RegisterContextHSA.h"
#include "UnwindHSA.h"
#include "Plugins/LanguageRuntime/HSA/HSARuntime/HSARuntime.h"

#define DECLARE_REGISTER_INFOS_HSA_STRUCT
#include "RegisterInfosHSA.h"
#undef DECLARE_REGISTER_INFOS_HSA_STRUCT

using namespace lldb;
using namespace lldb_private;

static const
uint32_t g_gpr_regnums[] =
{
    gpr_pc_hsa,
    gpr_fp_hsa,
    gpr_mp_hsa,
};


// Number of register sets provided by this context.
enum
{
    k_num_register_sets = 1
};

static const RegisterSet
g_reg_sets_hsa[k_num_register_sets] =
{
    { "General Purpose Registers",  "gpr", 3, g_gpr_regnums }
};


RegisterContextHSA::~RegisterContextHSA()
{
}

static const lldb_private::RegisterInfo *
GetRegisterInfoPtr ()
{
    return g_register_infos_hsa;
}

static uint32_t
GetRegisterInfoCount()
{
    return static_cast<uint32_t>(sizeof(g_register_infos_hsa) / sizeof(g_register_infos_hsa[0]));
}

RegisterContextHSA::RegisterContextHSA(Thread &thread, uint32_t concrete_frame_idx) :
    RegisterContext(thread, concrete_frame_idx),
    m_register_info_p(GetRegisterInfoPtr()),
    m_register_info_count(GetRegisterInfoCount())
{
}

void RegisterContextHSA::InvalidateAllRegisters() {

}

const lldb_private::RegisterInfo *
RegisterContextHSA::GetRegisterInfoAtIndex(size_t reg)
{
    return m_register_info_p + reg;
}

size_t
RegisterContextHSA::GetRegisterSetCount() {
    return 1;
}

const RegisterSet*
RegisterContextHSA::GetRegisterSet(size_t reg) {
    if (reg == 0) 
        return g_reg_sets_hsa + reg;
    else
        return nullptr;
}


size_t
RegisterContextHSA::GetRegisterCount()
{
    return m_register_info_count;
}

bool 
RegisterContextHSA::ReadRegister(const RegisterInfo* reg_info, RegisterValue &reg_value) {
    ReadAllRegisters();

    const uint32_t reg_num = reg_info->kinds[eRegisterKindLLDB];
    reg_value = m_registers[reg_num];
    return true;
}

bool 
RegisterContextHSA::WriteRegister(const RegisterInfo* reg_info, const RegisterValue &reg_value) {
    return false;
}

bool RegisterContextHSA::ReadAllRegisters() {
    ProcessSP process_sp (CalculateProcess());
    if (process_sp) {
        UnwindHSA unwinder (m_thread);

        lldb::addr_t pc;
        lldb::addr_t fp;
        unwinder.DoGetFrameInfoAtIndex(m_concrete_frame_idx, pc, fp);
        m_registers[0] = pc;
        m_registers[1] = fp;
        m_registers[2] = 0;

        return true;
    }

    return false;
}

uint32_t
RegisterContextHSA::ConvertRegisterKindToRegisterNumber (lldb::RegisterKind kind, uint32_t num)
{
    return false;
}
