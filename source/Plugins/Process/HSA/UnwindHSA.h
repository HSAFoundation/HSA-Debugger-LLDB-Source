//===-- UnwindHSA.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_UnwindHSA_h_
#define lldb_UnwindHSA_h_

// C Includes
// C++ Includes
#include <vector>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-public.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Symbol/FuncUnwinders.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Unwind.h"


namespace lldb_private {

class RegisterContextHSA;
class HSARuntime;

class UnwindHSA : public lldb_private::Unwind
{
public: 
    UnwindHSA (lldb_private::Thread &thread);

    ~UnwindHSA() override = default;

protected:
    friend class lldb_private::RegisterContextHSA;

    void
    DoClear() override
    {

    }

    uint32_t
    DoGetFrameCount() override;

    bool
    DoGetFrameInfoAtIndex(uint32_t frame_idx,
                          lldb::addr_t& cfa,
                          lldb::addr_t& start_pc) override;
    
    lldb::RegisterContextSP
    DoCreateRegisterContextForFrame(lldb_private::StackFrame *frame) override;


private:
    HSARuntime* m_runtime;

    //------------------------------------------------------------------
    // For UnwindHSA only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (UnwindHSA);
};

} // namespace lldb_private

#endif // lldb_UnwindHSA_h_
