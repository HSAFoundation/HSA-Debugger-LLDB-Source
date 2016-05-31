#include<fstream>//===-- NativeBreakpointHSA.cpp ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NativeBreakpointHSA.h"

#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Host/Debug.h"
#include "lldb/Host/Mutex.h"

#include "lldb/Host/common/NativeProcessProtocol.h"
#include "Plugins/LanguageRuntime/HSA/HSARuntime/NativeHSADebug.h"

using namespace lldb_private;

// -------------------------------------------------------------------
// static members
// -------------------------------------------------------------------

Error
NativeBreakpointHSA::CreateSoftwareBreakpoint (NativeProcessProtocol &process, NativeHSADebug& hsa_debug, lldb::addr_t addr, size_t size_hint, NativeBreakpointSP &breakpoint_sp)
{
    Log *log (GetLogIfAnyCategoriesSet (LIBLLDB_LOG_BREAKPOINTS));
    if (log)
        log->Printf ("NativeBreakpointHSA::%s addr = 0x%" PRIx64, __FUNCTION__, addr);

    // Validate the address.
    if (addr == LLDB_INVALID_ADDRESS)
        return Error ("NativeBreakpointHSA::%s invalid load address specified.", __FUNCTION__);

    // Enable the breakpoint.
    hsa_debug.SetBreakpoint(addr);

    if (log)
        log->Printf ("NativeBreakpointHSA::%s addr = 0x%" PRIx64 " -- SUCCESS", __FUNCTION__, addr);

    // Set the breakpoint and verified it was written properly.  Now
    // create a breakpoint remover that understands how to undo this
    // breakpoint.
    breakpoint_sp.reset (new NativeBreakpointHSA (process, hsa_debug, addr));
    return Error ();
}

// -------------------------------------------------------------------
// instance-level members
// -------------------------------------------------------------------

NativeBreakpointHSA::NativeBreakpointHSA (NativeProcessProtocol &process, NativeHSADebug& hsa_debug, lldb::addr_t addr) : 
    NativeBreakpoint (addr),
    m_process (process),
    m_hsa_debug (hsa_debug)
{
}

Error
NativeBreakpointHSA::DoEnable ()
{
    Log *log (GetLogIfAnyCategoriesSet (LIBLLDB_LOG_BREAKPOINTS));
    if (log)
        log->Printf ("NativeBreakpointHSA::%s addr = 0x%" PRIx64, __FUNCTION__, m_addr);


    m_hsa_debug.EnableBreakpoint(m_addr);
    return Error();
}

Error
NativeBreakpointHSA::DoDisable ()
{
    Error error;
    assert (m_addr && (m_addr != LLDB_INVALID_ADDRESS) && "can't remove a software breakpoint for an invalid address");

    Log *log (GetLogIfAnyCategoriesSet (LIBLLDB_LOG_BREAKPOINTS));
    if (log)
        log->Printf ("NativeBreakpointHSA::%s addr = 0x%" PRIx64, __FUNCTION__, m_addr);

    m_hsa_debug.DisableBreakpoint(m_addr);

    if (log && error.Fail ())
        log->Printf ("NativeBreakpointHSA::%s addr = 0x%" PRIx64 " -- FAILED: %s",
                __FUNCTION__,
                m_addr,
                error.AsCString());
    return error;
}

bool
NativeBreakpointHSA::IsSoftwareBreakpoint () const
{
    return true;
}

