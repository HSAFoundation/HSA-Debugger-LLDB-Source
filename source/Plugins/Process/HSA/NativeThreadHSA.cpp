//===-- NativeThreadHSA.cpp --------------------------------- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NativeThreadHSA.h"
#include "NativeRegisterContextHSA.h"

#include <signal.h>
#include <sstream>

#include "lldb/Host/HostNativeThread.h"
#include "lldb/Utility/LLDBAssert.h"
#include "lldb/Core/Log.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/Core/State.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"

#include "Plugins/Process/POSIX/CrashReason.h"
#include "Plugins/Process/Linux/NativeThreadLinux.h"
#include "Plugins/LanguageRuntime/HSA/HSARuntime/NativeHSADebug.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_linux;

namespace
{
    void LogThreadStopInfo (Log &log, const ThreadStopInfo &stop_info, const char *const header)
    {
        switch (stop_info.reason)
        {
            case eStopReasonNone:
                log.Printf ("%s: %s no stop reason", __FUNCTION__, header);
                return;
            case eStopReasonTrace:
                log.Printf ("%s: %s trace, stopping signal 0x%" PRIx32, __FUNCTION__, header, stop_info.details.signal.signo);
                return;
            case eStopReasonBreakpoint:
                log.Printf ("%s: %s breakpoint, stopping signal 0x%" PRIx32, __FUNCTION__, header, stop_info.details.signal.signo);
                return;
            case eStopReasonWatchpoint:
                log.Printf ("%s: %s watchpoint, stopping signal 0x%" PRIx32, __FUNCTION__, header, stop_info.details.signal.signo);
                return;
            case eStopReasonSignal:
                log.Printf ("%s: %s signal 0x%02" PRIx32, __FUNCTION__, header, stop_info.details.signal.signo);
                return;
            case eStopReasonException:
                log.Printf ("%s: %s exception type 0x%02" PRIx64, __FUNCTION__, header, stop_info.details.exception.type);
                return;
            case eStopReasonExec:
                log.Printf ("%s: %s exec, stopping signal 0x%" PRIx32, __FUNCTION__, header, stop_info.details.signal.signo);
                return;
            case eStopReasonPlanComplete:
                log.Printf ("%s: %s plan complete", __FUNCTION__, header);
                return;
            case eStopReasonThreadExiting:
                log.Printf ("%s: %s thread exiting", __FUNCTION__, header);
                return;
            case eStopReasonInstrumentation:
                log.Printf ("%s: %s instrumentation", __FUNCTION__, header);
                return;
            default:
                log.Printf ("%s: %s invalid stop reason %" PRIu32, __FUNCTION__, header, static_cast<uint32_t> (stop_info.reason));
        }
    }
}

NativeThreadHSA::NativeThreadHSA (NativeProcessProtocol *process, lldb::tid_t tid, NativeHSADebug& hsa_debug) :
    NativeThreadProtocol (process, tid, ArchSpec("amdgcn")),
    m_state (StateType::eStateStopped),
    m_stop_info (),
    m_reg_context_sp (),
    m_stop_description (),
    m_hsa_debug (hsa_debug)
{

}

std::string
NativeThreadHSA::GetName()
{
    return "HSA Wavefront";
}

lldb::StateType
NativeThreadHSA::GetState ()
{
    return m_state;
}


bool
NativeThreadHSA::GetStopReason (ThreadStopInfo &stop_info, std::string& description)
{
    Log *log (GetLogIfAllCategoriesSet (LIBLLDB_LOG_THREAD));

    description.clear();

    switch (m_state)
    {
    case eStateStopped:
    case eStateCrashed:
    case eStateExited:
    case eStateSuspended:
    case eStateUnloaded:
        if (log)
            LogThreadStopInfo (*log, m_stop_info, "m_stop_info in thread:");
        stop_info = m_stop_info;
        description = m_stop_description;
        if (log)
            LogThreadStopInfo (*log, stop_info, "returned stop_info:");

        return true;

    case eStateInvalid:
    case eStateConnected:
    case eStateAttaching:
    case eStateLaunching:
    case eStateRunning:
    case eStateStepping:
    case eStateDetached:
        if (log)
        {
            log->Printf ("NativeThreadHSA::%s tid %" PRIu64 " in state %s cannot answer stop reason",
                    __FUNCTION__, GetID (), StateAsCString (m_state));
        }
        return false;
    }
    llvm_unreachable("unhandled StateType!");
}

NativeRegisterContextSP
NativeThreadHSA::GetRegisterContext ()
{
    // Return the register context if we already created it.
    if (m_reg_context_sp)
        return m_reg_context_sp;

    NativeProcessProtocolSP m_process_sp = m_process_wp.lock ();
    if (!m_process_sp)
        return NativeRegisterContextSP ();

    m_reg_context_sp.reset(new NativeRegisterContextHSA(*this, GetID(), m_hsa_debug));
    return m_reg_context_sp;
}

Error
NativeThreadHSA::SetWatchpoint (lldb::addr_t addr, size_t size, uint32_t watch_flags, bool hardware)
{
    return Error ("not implemented");
}

Error
NativeThreadHSA::RemoveWatchpoint (lldb::addr_t addr)
{
    return Error ("not implemented");
}


void
NativeThreadHSA::SetStepping () 
{
    m_state = eStateStepping;
    m_stop_info.reason = StopReason::eStopReasonNone;
}


void
NativeThreadHSA::SetRunning () 
{
    m_state = eStateRunning;
    m_stop_info.reason = StopReason::eStopReasonNone;
}

void
NativeThreadHSA::SetStoppedByBreakpoint ()
{
    const StateType new_state = StateType::eStateStopped;
    m_state = new_state;

    m_stop_info.reason = StopReason::eStopReasonBreakpoint;
    m_stop_info.details.signal.signo = SIGTRAP;
    m_stop_description.clear();
}

void
NativeThreadHSA::SetStoppedWithNoReason ()
{
    const StateType new_state = StateType::eStateStopped;
    m_state = new_state;

    m_stop_info.reason = StopReason::eStopReasonNone;
    m_stop_info.details.signal.signo = 0;
}
