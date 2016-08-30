//===-- UnwindHSA.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/Module.h"
#include "lldb/Core/Log.h"
#include "lldb/Symbol/FuncUnwinders.h"
#include "lldb/Symbol/Function.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/ABI.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"

#include "Plugins/LanguageRuntime/HSA/HSARuntime/HSARuntime.h"
#include "Plugins/SymbolFile/AMDHSA/SymbolFileAMDHSA.h"

#include "UnwindHSA.h"
#include "RegisterContextHSA.h"

using namespace lldb;
using namespace lldb_private;

UnwindHSA::UnwindHSA (Thread &thread) :
  Unwind (thread),
  m_runtime ((HSARuntime *)thread.GetProcess()->GetLanguageRuntime(eLanguageTypeObjC))

{}

uint32_t
UnwindHSA::DoGetFrameCount() {
    return 1;
}

bool
UnwindHSA::DoGetFrameInfoAtIndex(uint32_t frame_idx,
				 lldb::addr_t& cfa,
				 lldb::addr_t& start_pc) {
    auto thread_reg = m_thread.GetRegisterContext();
    auto pc = thread_reg->GetPC();

    auto target_sp = m_thread.CalculateTarget();
    auto& images = target_sp->GetImages();

    ModuleSP module_sp;
    for (unsigned i=0; i < images.GetSize(); ++i) {
        auto module = images.GetModuleAtIndex(i);
        if (module) {
            if (module->GetArchitecture().GetMachine() == llvm::Triple::amdgcn) {
                module_sp = module;
                break;
            }
        }
    }

    if (!module_sp)
        return false;

    auto sym_vendor = module_sp->GetSymbolVendor();
    if (sym_vendor) {
        auto sym_file = static_cast<SymbolFileAMDHSA*>(sym_vendor->GetSymbolFile());
        if (sym_file) {
            auto dbginfo = sym_file->GetDbgInfo();

            std::vector<HwDbgInfo_frame_context> frames (100);
            size_t n_frames;
            hwdbginfo_addr_call_stack(dbginfo, pc, frames.size(), frames.data(), &n_frames);
            
            if (frame_idx < n_frames) {
                HwDbgInfo_addr pc, fp, mp;
                HwDbgInfo_code_location loc;
                std::vector<char> func_name (100);
                auto err = hwdbginfo_frame_context_details(frames[frame_idx], &pc, &fp, &mp, &loc, func_name.size(), func_name.data(), &n_frames);
                
                if (err != HWDBGINFO_E_SUCCESS) {
                    return false;
                }

                cfa = fp;
                start_pc = pc;
                return true;
            }
        }
    }    

    return false;
}

RegisterContextSP
UnwindHSA::DoCreateRegisterContextForFrame(StackFrame *frame) {
  return RegisterContextSP(new RegisterContextHSA(*frame->GetThread(), frame->GetFrameIndex()));
}

