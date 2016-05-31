//===-- HSABreakpointResolver.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_HSABreakpointResolver_h_
#define liblldb_HSABreakpointResolver_h_

#include "lldb/lldb-private.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/Stream.h"
#include "lldb/Symbol/SymbolContext.h"
#include "lldb/Breakpoint/BreakpointResolver.h"

namespace lldb_private {

class HSABreakpointResolver : public BreakpointResolver
{
public:
    HSABreakpointResolver(Breakpoint *bkpt, ConstString name):
                         BreakpointResolver (bkpt, BreakpointResolver::NameResolver),
                         m_kernel_name(name),
                         m_line(0),
                         m_type(Type::KernelEntry)
    {
    }

    HSABreakpointResolver(Breakpoint *bkpt, ConstString name, size_t line):
                         BreakpointResolver (bkpt, BreakpointResolver::NameResolver),
                         m_kernel_name(name),
                         m_line(line),
                         m_type(Type::KernelLine)

    {
    }

    HSABreakpointResolver(Breakpoint *bkpt):
                         BreakpointResolver (bkpt, BreakpointResolver::NameResolver),
                         m_kernel_name(""),
                         m_line(0),
                         m_type(Type::AllKernels)

    {
    }

    void
    GetDescription(Stream *strm) override
    {
        if (strm)
            strm->Printf("HSA kernel breakpoint for '%s'", m_kernel_name.AsCString());
    }

    void
    Dump(Stream *s) const override
    {
    }

    Searcher::CallbackReturn
    SearchCallback(SearchFilter &filter,
                   SymbolContext &context,
                   Address *addr,
                   bool containing) override;

    Searcher::Depth
    GetDepth() override
    {
        return Searcher::eDepthModule;
    }

    lldb::BreakpointResolverSP
    CopyForBreakpoint(Breakpoint &breakpoint) override
    {
        lldb::BreakpointResolverSP ret_sp(new HSABreakpointResolver(&breakpoint, m_kernel_name));
        return ret_sp;
    }

protected:
    enum class Type {
        AllKernels, KernelEntry, KernelLine
    };

    ConstString m_kernel_name;
    size_t m_line;
    Type m_type;
};

} // namespace lldb_private

#endif // liblldb_HSABreakpointResolver_h_
