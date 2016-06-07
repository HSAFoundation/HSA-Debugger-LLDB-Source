//===-- ThreadGDBRemoteHSA.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include "ThreadGDBRemoteHSA.h"
#include "ProcessGDBRemote.h"
#include "Plugins/Process/HSA/ThreadPlanStepOverHSA.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

RegisterContextSP
ThreadGDBRemoteHSA::CreateRegisterContextForFrame (StackFrame *frame)
{
    lldb::RegisterContextSP reg_ctx_sp;
    uint32_t concrete_frame_idx = 0;
    
    if (frame)
        concrete_frame_idx = frame->GetConcreteFrameIndex ();

    
    if (concrete_frame_idx == 0)
    {
        ProcessSP process_sp (GetProcess());
        if (process_sp)
        {
            ProcessGDBRemote *gdb_process = static_cast<ProcessGDBRemote *>(process_sp.get());
            // read_all_registers_at_once will be true if 'p' packet is not supported.
            bool read_all_registers_at_once = !gdb_process->GetGDBRemote().GetpPacketSupported (GetID());

            auto& reg_info = gdb_process->m_hsa_register_info;
            reg_ctx_sp.reset (new GDBRemoteRegisterContextHSA (*this, concrete_frame_idx, reg_info, read_all_registers_at_once));
        }
    }
    else
    {
        Unwind *unwinder = GetUnwinder ();
        if (unwinder)
            reg_ctx_sp = unwinder->CreateRegisterContextForFrame (frame);
    }
    return reg_ctx_sp;
}


ThreadPlanSP
ThreadGDBRemoteHSA::QueueThreadPlanForStepOverRange(bool abort_other_plans,
                                                    const AddressRange &range,
                                                    const SymbolContext &addr_context,
                                                    lldb::RunMode stop_other_threads,
                                                    LazyBool step_out_avoids_code_withoug_debug_info)
{
    ThreadPlanSP thread_plan_sp;
    thread_plan_sp.reset (new ThreadPlanStepOverHSA (*this, addr_context));
    
    QueueThreadPlan (thread_plan_sp, abort_other_plans);
    return thread_plan_sp;
}
