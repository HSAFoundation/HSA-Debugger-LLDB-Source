//===-- NativeThreadHSA.h ----------------------------------- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_NativeThreadHSA_H_
#define liblldb_NativeThreadHSA_H_

#include "lldb/lldb-private-forward.h"
#include "lldb/Host/common/NativeThreadProtocol.h"
#include "Plugins/Process/Linux/NativeThreadLinux.h"

#include <map>
#include <memory>
#include <string>

namespace lldb_private {
    class NativeHSADebug;
    class NativeProcessHSA;

    class NativeThreadHSA : public NativeThreadProtocol
    {
        friend class NativeProcessLinux;

    public:
        NativeThreadHSA (NativeProcessProtocol *process, lldb::tid_t tid, NativeHSADebug& hsa_debug);

        // ---------------------------------------------------------------------
        // NativeThreadProtocol Interface
        // ---------------------------------------------------------------------
        std::string
        GetName() override;

        lldb::StateType
        GetState () override;

        void
        SetStepping ();

        void
        SetRunning ();

        void
        SetStoppedByBreakpoint ();

        void
        SetStoppedWithNoReason ();

        bool
        GetStopReason (ThreadStopInfo &stop_info, std::string& description) override;

        NativeRegisterContextSP
        GetRegisterContext () override;

        Error
        SetWatchpoint (lldb::addr_t addr, size_t size, uint32_t watch_flags, bool hardware) override;

        Error
        RemoveWatchpoint (lldb::addr_t addr) override;

    private:
        // ---------------------------------------------------------------------
        // Member Variables
        // ---------------------------------------------------------------------
        lldb::StateType m_state;
        ThreadStopInfo m_stop_info;
        NativeRegisterContextSP m_reg_context_sp;
        std::string m_stop_description;
        using WatchpointIndexMap = std::map<lldb::addr_t, uint32_t>;
        WatchpointIndexMap m_watchpoint_index_map;
        NativeHSADebug& m_hsa_debug;
    };

    typedef std::shared_ptr<NativeThreadHSA> NativeThreadHSASP;
} // namespace lldb_private

#endif // #ifndef liblldb_NativeThreadHSA_H_
