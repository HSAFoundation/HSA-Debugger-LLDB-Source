//===-- RegisterContextHSA.h -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_RegisterContextHSA_h_
#define liblldb_RegisterContextHSA_h_

#include "lldb/lldb-private.h"
#include "lldb/Target/RegisterContext.h"
#include "../Utility/RegisterInfoInterface.h"

namespace lldb_private {

class RegisterContextHSA
    : public lldb_private::RegisterContext
{
public:
    RegisterContextHSA(Thread &thread, uint32_t concrete_frame_idx);

    ~RegisterContextHSA() override;

    void
    InvalidateAllRegisters() override;
    
    size_t
    GetRegisterCount() override;
    
    const RegisterInfo *
    GetRegisterInfoAtIndex(size_t reg) override;
    
    size_t
    GetRegisterSetCount() override;
    
    const RegisterSet *
    GetRegisterSet(size_t reg_set) override;
    
    bool
    ReadRegister(const RegisterInfo *reg_info, RegisterValue &reg_value) override;
    
    bool
    WriteRegister(const RegisterInfo *reg_info, const RegisterValue &reg_value) override;

    uint32_t
    ConvertRegisterKindToRegisterNumber(lldb::RegisterKind kind, uint32_t num) override;

private:
    bool
    ReadAllRegisters();
  

    const lldb_private::RegisterInfo *m_register_info_p;
    uint32_t m_registers[3];
    uint32_t m_register_info_count;
};

} // namespace lldb_private

#endif  // liblldb_RegisterContextHSA_h_

