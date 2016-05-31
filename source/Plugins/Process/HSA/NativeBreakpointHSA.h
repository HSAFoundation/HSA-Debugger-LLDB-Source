//===-- NativeBreakpointHSA.h ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_NativeBreakpointHSA_h_
#define liblldb_NativeBreakpointHSA_h_

#include "lldb/lldb-private-forward.h"
#include "lldb/Host/common/NativeBreakpoint.h"

namespace lldb_private
{
    class NativeHSADebug;

    class NativeBreakpointHSA : public NativeBreakpoint
    {
        friend class NativeBreakpointList;

    public:
        static Error
        CreateSoftwareBreakpoint (NativeProcessProtocol &process, NativeHSADebug& hsa_debug, lldb::addr_t addr, size_t size_hint, NativeBreakpointSP &breakpoint_spn);

        NativeBreakpointHSA (NativeProcessProtocol &process, NativeHSADebug& hsa_debug, lldb::addr_t addr);

    protected:
        Error
        DoEnable () override;

        Error
        DoDisable () override;

        bool
        IsSoftwareBreakpoint () const override;

    private:
        NativeProcessProtocol &m_process;
        NativeHSADebug &m_hsa_debug;

        static Error
        EnableNativeBreakpointHSA (NativeProcessProtocol &process, lldb::addr_t addr, size_t bp_opcode_size, const uint8_t *bp_opcode_bytes, uint8_t *saved_opcode_bytes);

    };
}

#endif // #ifndef liblldb_NativeBreakpointHSA_h_

