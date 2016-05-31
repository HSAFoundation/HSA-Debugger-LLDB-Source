//===-- ThreadPlanStepOverHSA.cpp -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ThreadPlanStepOverHSA.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/Stream.h"
#include "lldb/Symbol/Block.h"
#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/LineTable.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadPlanStepOut.h"
#include "lldb/Target/ThreadPlanStepThrough.h"

#include "Plugins/SymbolFile/AMDHSA/SymbolFileAMDHSA.h"

using namespace lldb_private;
using namespace lldb;

ThreadPlanStepOverHSA::ThreadPlanStepOverHSA
(
    Thread &thread,
    const SymbolContext &addr_context
) :
    ThreadPlan (ThreadPlan::eKindStepOverRange, "Stepping over HSA", thread, eVoteNoOpinion, eVoteNoOpinion),
    m_addr_context (addr_context)
{
}

ThreadPlanStepOverHSA::~ThreadPlanStepOverHSA ()
{
}

void
ThreadPlanStepOverHSA::GetDescription (Stream *s, lldb::DescriptionLevel level)
{
    if (level == lldb::eDescriptionLevelBrief)
    {
        s->Printf("step over");
        return;
    }
    s->Printf ("Stepping over");

    if (m_addr_context.line_entry.IsValid())
    {
        s->Printf (" line ");
        m_addr_context.line_entry.DumpStopContext (s, false);
    }

    s->PutChar('.');
}

bool
ThreadPlanStepOverHSA::ValidatePlan (Stream *error)
{
    return true;
}

bool
ThreadPlanStepOverHSA::ShouldStop (Event *event_ptr)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));

    if (IsPlanComplete()) return true;

    //Check if we haven't moved yet
    StackFrame *frame = m_thread.GetStackFrameAtIndex(0).get();
        
    SymbolContext new_context(frame->GetSymbolContext(eSymbolContextEverything));
    if (m_addr_context.line_entry.IsValid() && new_context.line_entry.IsValid())
    {
        if (m_addr_context.line_entry.file == new_context.line_entry.file)
        {
            if (m_addr_context.line_entry.line == new_context.line_entry.line)
            {
                ClearMomentaryBreakpoints();
                return false;
            }
        }
    }

    if (log)
    {
        StreamString s;
        s.Address (m_thread.GetRegisterContext()->GetPC(), 
                   GetTarget().GetArchitecture().GetAddressByteSize());
        log->Printf("ThreadPlanStepOverHSA reached %s.", s.GetData());
    }

    if (HitMomentaryBreakpoint(GetPrivateStopInfo()))
    {
        ClearMomentaryBreakpoints();
        SetPlanComplete();
        return true;
    }

    ClearMomentaryBreakpoints();
    SetMomentaryBreakpoints();
    return false;
}

Vote ThreadPlanStepOverHSA::ShouldReportStop (Event *event_ptr)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));

    const Vote vote = IsPlanComplete() ? eVoteYes : eVoteNo;
    if (log)
        log->Printf ("ThreadPlanStepOverHSA::ShouldReportStop() returning vote %i\n", vote);
    return vote;
}

bool ThreadPlanStepOverHSA::StopOthers ()
{
    return false;
}

bool
ThreadPlanStepOverHSA::DoPlanExplainsStop (Event *event_ptr)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_STEP));
    StopInfoSP stop_info_sp = GetPrivateStopInfo ();
    bool return_value;
    
    if (stop_info_sp)
    {
        StopReason reason = stop_info_sp->GetStopReason();

        if (reason == eStopReasonTrace)
        {
            return_value = true;
        }
        else if (reason == eStopReasonBreakpoint)
        {
            if (HitMomentaryBreakpoint(GetPrivateStopInfo()))
                return_value = true;
            else
                return_value = false;
        }
        else
        {
            if (log)
                log->PutCString ("ThreadPlanStepOverHSA got asked if it explains the stop for some reason other than step.");
            return_value = false;
        }
    }
    else
        return_value = true;

    return return_value;
}

void
ThreadPlanStepOverHSA::DidPush()
{
    SetMomentaryBreakpoints();
}

bool
ThreadPlanStepOverHSA::WillStop ()
{
    return true;
}

void
ThreadPlanStepOverHSA::SetMomentaryBreakpoints() 
{
    Target& target = GetTarget();

    auto& images = target.GetImages();

    ModuleSP module;
    for (unsigned i=0; i < images.GetSize(); ++i) {
        auto module_sp = images.GetModuleAtIndex(i);
        if (module_sp) {
            if (module_sp->GetArchitecture().GetMachine() == llvm::Triple::hsail) {
                module = module_sp;
                break;
            }
        }
    }

    if (module) {
        auto sym_vendor = module->GetSymbolVendor();
        if (sym_vendor) {
            auto sym_file = static_cast<SymbolFileAMDHSA*>(sym_vendor->GetSymbolFile());
            if (sym_file) {
                auto pc = GetThread().GetRegisterContext()->GetPC();
                auto addrs = sym_file->GetStepOverAddresses(pc);

                for (auto addr : addrs) {
                    const bool is_hardware = true; //used for "momentary" breakpoints
                    const bool is_internal = true;
                    auto bp = target.CreateBreakpoint(addr, is_internal, is_hardware);
                    m_momentary_breakpoints.push_back(bp);
                }
            }
        }
    }
}

void
ThreadPlanStepOverHSA::ClearMomentaryBreakpoints() 
{
    for (auto bp : m_momentary_breakpoints) {
        GetTarget().RemoveBreakpointByID(bp->GetID());
    }
}

bool
ThreadPlanStepOverHSA::HitMomentaryBreakpoint(StopInfoSP stop_info_sp) 
{
    if (stop_info_sp->GetStopReason() != eStopReasonBreakpoint) 
        return false;

    break_id_t bp_site_id = stop_info_sp->GetValue();
    BreakpointSiteSP bp_site_sp = m_thread.GetProcess()->GetBreakpointSiteList().FindByID(bp_site_id);

    if (!bp_site_sp) return false;
    
    for (auto bp : m_momentary_breakpoints) {
        if (bp_site_sp->IsBreakpointAtThisSite(bp->GetID()))
            return true;
    }

    return false;
}

StateType
ThreadPlanStepOverHSA::GetPlanRunState()
{
    return eStateStepping;
}
