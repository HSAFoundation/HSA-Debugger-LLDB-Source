//===-- HSARuntime.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_HSARuntime_h_
#define liblldb_HSARuntime_h_

#include "lldb/lldb-private.h"
#include "lldb/Target/CPPLanguageRuntime.h"
#include "lldb/Target/LanguageRuntime.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/Type.h"

namespace lldb_private {

class HSARuntime : public lldb_private::CPPLanguageRuntime
{
public:
    ~HSARuntime();


    void
    ModulesDidLoad (const lldb_private::ModuleList &module_list) override;

    void SetBreakAllKernels (bool do_break);


    //------------------------------------------------------------------
    // Static Functions
    //------------------------------------------------------------------
    static void Initialize();

    static void Terminate();

    bool IsVTableName(const char *name) override;

    bool GetDynamicTypeAndAddress(ValueObject &in_value, lldb::DynamicValueType use_dynamic,
                                  TypeAndOrName &class_type_or_name, Address &address,
                                  Value::ValueType &value_type) override;
    
    TypeAndOrName
    FixUpDynamicType(const TypeAndOrName& type_and_or_name,
                     ValueObject& static_value) override;

    bool CouldHaveDynamicValue(ValueObject &in_value) override;

    lldb::BreakpointResolverSP CreateExceptionResolver(Breakpoint *bkpt, bool catch_bp, bool throw_bp) override;

    static lldb_private::LanguageRuntime *CreateInstance(Process *process, lldb::LanguageType language);

    static lldb::CommandObjectSP GetCommandObject(CommandInterpreter& interpreter);

    static lldb_private::ConstString GetPluginNameStatic();

    //------------------------------------------------------------------
    // PluginInterface protocol
    //------------------------------------------------------------------

    lldb_private::ConstString GetPluginName() override;

    uint32_t GetPluginVersion() override;

private:
    bool IsHSAModule (const lldb_private::Module& module_sp);

    HSARuntime(Process *process);
    bool m_break_all_kernels = false;
};

} // namespace lldb_private

#endif // liblldb_HSARuntime_h_
