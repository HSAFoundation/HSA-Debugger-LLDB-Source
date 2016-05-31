//===-- ThreadPlanStepOverHSA.h -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ThreadPlanStepOverHSA_h_
#define liblldb_ThreadPlanStepOverHSA_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadPlan.h"

namespace lldb_private {

class ThreadPlanStepOverHSA : public ThreadPlan
{
public:
    ThreadPlanStepOverHSA (Thread &thread, const SymbolContext& addr_context);
                           

    ~ThreadPlanStepOverHSA() override;

    void GetDescription(Stream *s, lldb::DescriptionLevel level) override;
    bool ValidatePlan(Stream *error) override;
    bool ShouldStop(Event *event_ptr) override;
    Vote ShouldReportStop(Event *event_ptr) override;
    bool StopOthers() override;
    lldb::StateType GetPlanRunState() override;
    bool WillStop() override;
    void DidPush() override;

protected:
    bool DoPlanExplainsStop (Event *event_ptr) override;

private:
    void SetMomentaryBreakpoints();
    void ClearMomentaryBreakpoints();
    bool HitMomentaryBreakpoint(lldb::StopInfoSP stop_info_sp);

    SymbolContext m_addr_context;
    std::vector<lldb::BreakpointSP> m_momentary_breakpoints;

    DISALLOW_COPY_AND_ASSIGN (ThreadPlanStepOverHSA);
};

} // namespace lldb_private

#endif // liblldb_ThreadPlanStepOverHSA_h_
