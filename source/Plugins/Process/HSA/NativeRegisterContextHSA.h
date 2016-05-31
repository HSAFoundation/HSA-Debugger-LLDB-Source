//===-- NativeRegisterContextHSA.h ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_NativeRegisterContextHSA_h
#define lldb_NativeRegisterContextHSA_h

#include "lldb/Host/common/NativeRegisterContext.h"

namespace lldb_private {
    class NativeHSADebug;
    class NativeProcessLinux;

    class NativeRegisterContextHSA : public NativeRegisterContext
    {
    public:
        NativeRegisterContextHSA (NativeThreadProtocol &native_thread,
                                  uint32_t concrete_frame_idx,
                                  NativeHSADebug& hsa_debug);

        ~NativeRegisterContextHSA(){}

        static NativeRegisterContextHSA*
	CreateNativeRegisterContextHSA(NativeThreadProtocol &native_thread,
                                       uint32_t concrete_frame_idx,
                                       NativeHSADebug& hsa_debug);

        virtual uint32_t
        GetRegisterCount () const override;

        virtual uint32_t
        GetUserRegisterCount () const override;

        uint32_t
        GetRegisterSetCount () const override;

        const RegisterSet *
        GetRegisterSet (uint32_t set_index) const override;

        Error
        ReadRegister (const RegisterInfo *reg_info, RegisterValue &reg_value) override;

        Error
        WriteRegister (const RegisterInfo *reg_info, const RegisterValue &reg_value) override;

        Error
        ReadAllRegisterValues (lldb::DataBufferSP &data_sp) override;

        Error
        WriteAllRegisterValues (const lldb::DataBufferSP &data_sp) override;

        const RegisterInfo *
        GetRegisterInfoAtIndex (uint32_t reg) const override;

    private:
        NativeHSADebug& m_hsa_debug;
    };

} // namespace lldb_private


#endif // #ifndef lldb_NativeRegisterContextHSA_h
