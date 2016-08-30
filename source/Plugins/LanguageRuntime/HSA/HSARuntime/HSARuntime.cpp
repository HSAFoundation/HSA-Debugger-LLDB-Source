//===-- HSARuntime.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
// Other libraries and framework includes
#include "HSARuntime.h"
#include "CommandObjectHSA.h"

#include "lldb/lldb-private.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/ModuleList.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Target/Target.h"
#include "Plugins/SymbolFile/AMDHSA/SymbolFileAMDHSA.h"
#include "HSABreakpointResolver.h"

using namespace lldb;
using namespace lldb_private;

void
HSARuntime::Initialize()
{
    PluginManager::RegisterPlugin(GetPluginNameStatic(), "HSA support", CreateInstance, GetCommandObject);
}

void
HSARuntime::Terminate()
{
    PluginManager::UnregisterPlugin(CreateInstance);
}

bool
HSARuntime::IsVTableName(const char *name)
{
    return false;
}

bool
HSARuntime::GetDynamicTypeAndAddress(ValueObject &in_value, lldb::DynamicValueType use_dynamic,
                                              TypeAndOrName &class_type_or_name, Address &address,
                                              Value::ValueType &value_type)
{
    return false;
}

TypeAndOrName
HSARuntime::FixUpDynamicType (const TypeAndOrName& type_and_or_name,
                                       ValueObject& static_value)
{
    return type_and_or_name;
}

bool
HSARuntime::CouldHaveDynamicValue(ValueObject &in_value)
{
    return false;
}

lldb::BreakpointResolverSP
HSARuntime::CreateExceptionResolver(Breakpoint *bkpt, bool catch_bp, bool throw_bp)
{
    BreakpointResolverSP resolver_sp;
    return resolver_sp;
}


lldb_private::ConstString
HSARuntime::GetPluginNameStatic()
{
    static ConstString g_name("hsa");
    return g_name;
}

//------------------------------------------------------------------
// PluginInterface protocol
//------------------------------------------------------------------
lldb_private::ConstString
HSARuntime::GetPluginName()
{
    return GetPluginNameStatic();
}

uint32_t
HSARuntime::GetPluginVersion()
{
    return 1;
}


lldb::CommandObjectSP
HSARuntime::GetCommandObject(lldb_private::CommandInterpreter& interpreter)
{
    static CommandObjectSP command_object;
    if(!command_object)
    {
      command_object.reset(new CommandObjectHSA(interpreter));
    }
    return command_object;
}

HSARuntime::~HSARuntime() = default;

HSARuntime::HSARuntime (Process* process)
  : lldb_private::CPPLanguageRuntime(process)
{
  
}

LanguageRuntime *
HSARuntime::CreateInstance(Process *process, lldb::LanguageType language)
{
  return new HSARuntime(process);
}

void
HSARuntime::ModulesDidLoad (const ModuleList &module_list)
{
    Mutex::Locker locker (module_list.GetMutex ());

    size_t num_modules = module_list.GetSize();
    for (size_t i = 0; i < num_modules; i++)
    {
        auto mod = module_list.GetModuleAtIndex (i);
        if (IsHSAModule (*mod))
        {
            auto sym_vendor = mod->GetSymbolVendor();
            if (sym_vendor) {
                auto sym_file = sym_vendor->GetSymbolFile();
                if (sym_file) {
                    auto kernel_name = static_cast<SymbolFileAMDHSA*>(sym_file)->GetKernelName();

                    auto& target = GetProcess()->GetTarget();
                    SearchFilterSP filter_sp (new SearchFilterForUnconstrainedSearches(target.shared_from_this()));
                    BreakpointResolverSP resolver_sp (new HSABreakpointResolver(nullptr, kernel_name));
                    target.CreateBreakpoint(filter_sp, resolver_sp, false, false, false);
                }
            }

        }
    }
}


void 
HSARuntime::SetBreakAllKernels (bool do_break) {
    m_break_all_kernels = do_break;
}

bool 
HSARuntime::IsHSAModule (const Module& module) {
    return module.GetArchitecture().GetMachine() == llvm::Triple::amdgcn;
}
