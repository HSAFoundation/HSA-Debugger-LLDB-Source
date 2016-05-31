//===-- ThreadGDBRemoteHSA.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ThreadGDBRemoteHSA_h_
#define liblldb_ThreadGDBRemoteHSA_h_

// C Includes
// C++ Includes
#include <string>

// Other libraries and framework includes
// Project includes
#include "lldb/Core/StructuredData.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Thread.h"

#include "Plugins/Process/HSA/UnwindHSA.h"
#include "Plugins/Process/HSA/ThreadPlanStepOverHSA.h"

#include "ThreadGDBRemote.h"

class StringExtractor;

namespace lldb_private {
namespace process_gdb_remote {

class ProcessGDBRemoteHSA;

class ThreadGDBRemoteHSA : public ThreadGDBRemote
{
public:
    ThreadGDBRemoteHSA (Process &process, lldb::tid_t tid) 
    : ThreadGDBRemote (process, tid) {}

    ~ThreadGDBRemoteHSA(){}

    lldb::RegisterContextSP
    CreateRegisterContextForFrame (StackFrame *frame) override;

    Unwind *
    GetUnwinder () override { 
        if (m_unwinder_ap)
            return m_unwinder_ap.get();
        
        m_unwinder_ap.reset(new UnwindHSA (*this));
        return m_unwinder_ap.get();
    }

    bool IsHSAThread() const override {
        return true;
    }

    lldb::ThreadPlanSP
    QueueThreadPlanForStepOverRange(bool abort_other_plans,
                                    const AddressRange &range,
                                    const SymbolContext &addr_context,
                                    lldb::RunMode stop_other_threads,
                                    LazyBool step_out_avoids_code_withoug_debug_info);
};

} // namespace process_gdb_remote
} // namespace lldb_private

#endif // liblldb_ThreadGDBRemoteHSA_h_
