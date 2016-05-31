//===-- CommandObjectHSA.cpp -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CommandObjectHSA.h"
#include "HsaDebugPacket.h"
#include "HSARuntime.h"
#include "HSABreakpointResolver.h"
#include "CommunicationParams.h"
#include "lldb/Symbol/Function.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "llvm/ADT/Triple.h"
#include "lldb/Host/StringConvert.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Interpreter/OptionValueString.h"
#include "lldb/Interpreter/OptionValueUInt64.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Symbol/SymbolFile.h"
#include "Plugins/SymbolFile/AMDHSA/SymbolFileAMDHSA.h"
#include "Plugins/SymbolFile/AMDHSA/FacilitiesInterface.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <streambuf>

using namespace lldb;
using namespace lldb_private;

using hsa_pc = uint64_t;

/*static OptionEnumValueElement
data_breakpoint_enums[] =
{
    {HSADebugAPI::Read, "read", "Read operations only"},
    {HSADebugAPI::NonRead, "non-read", "Write or atomic operations only"},
    {HSADebugAPI::Atomic, "atomic", "Atomic operations only"},
    {HSADebugAPI::All, "all", "Read, write or atomic operations"},
    };*/


bool CurrentlyUnimplemented(CommandReturnObject &result)
{
    result.AppendError("Currently unimplemented");
    result.SetStatus (eReturnStatusFailed);
    return false;
}


lldb::addr_t GetFunctionAddr(Target& target, ConstString func_name) {
    SymbolContextList sc_list;
    target.GetImages().FindFunctions(func_name, eFunctionNameTypeAuto, true, true, false, sc_list);
                
    if (sc_list.GetSize() > 0) {
        auto func = sc_list[0].function;
        if (func)  {
            auto addr = func->GetAddressRange().GetBaseAddress();
            return addr.GetLoadAddress(&target);
        }
    }

    return LLDB_INVALID_ADDRESS;
}


//-------------------------------------------------------------------------
// CommandObjectHSAKernelSource
//-------------------------------------------------------------------------
#pragma mark Source

class CommandObjectHSAKernelSource : public CommandObjectParsed
{
public:
    CommandObjectHSAKernelSource (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "hsa kernel source",
                             "Output the source code of a given HSA kernel.",
                             NULL),
        m_options(interpreter)
	
    {
      
    }


    virtual
    ~CommandObjectHSAKernelSource () {}

    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter),
            m_kernel_name ("")
        {
        }


        virtual
        ~CommandOptions () {}

        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;

            switch (short_option)
            {
                case 'k':
                {
		    m_kernel_name = option_arg;
                }
		break;

                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }

            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_kernel_name = "";
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.
        static OptionDefinition g_option_table[];

        std::string m_kernel_name;
    };


protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
	return CurrentlyUnimplemented(result);
    }

private:
    CommandOptions m_options;	  
};

#pragma mark Source::CommandOptions
OptionDefinition
CommandObjectHSAKernelSource::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_1, false, "kernel", 'k', OptionParser::eRequiredArgument, NULL, NULL, 0, eArgTypeFunctionName,
        "Name of kernel to display the source of." },

    { 0, false, NULL, 0, 0, NULL, NULL, 0, eArgTypeNone, NULL }
};



//-------------------------------------------------------------------------
// CommandObjectHSAKernelList
//-------------------------------------------------------------------------
#pragma mark List

class CommandObjectHSAKernelList : public CommandObjectParsed
{
public:
    CommandObjectHSAKernelList (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "hsa kernel list",
                             "List some or all HSA kernels at configurable levels of detail.",
                             NULL)
    {
    }


    virtual
    ~CommandObjectHSAKernelList () {}

protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        return CurrentlyUnimplemented(result);
    }
};

//-------------------------------------------------------------------------
// CommandObjectMultiwordHSAKernel
//-------------------------------------------------------------------------
#pragma mark Kernel

class CommandObjectMultiwordHSAKernel : public CommandObjectMultiword
{
public:
    CommandObjectMultiwordHSAKernel (CommandInterpreter &interpreter) :
        CommandObjectMultiword (interpreter,
                             "hsa kernel",
                             "A set of commands for operating on HSA kernels",
                             "hsa kernel <command> [<command-options>]")
    {
        CommandObjectSP list_command_object (new CommandObjectHSAKernelList (interpreter));
        CommandObjectSP source_command_object (new CommandObjectHSAKernelSource (interpreter));

        list_command_object->SetCommandName ("hsa kernel list");
        source_command_object->SetCommandName ("hsa kernel source");

        LoadSubCommand ("list",       list_command_object);
        LoadSubCommand ("source",     source_command_object);
    }
    virtual
    ~CommandObjectMultiwordHSAKernel () {}
};

class CommandObjectHSABreakpointAll : public CommandObjectParsed
{
public:
    CommandObjectHSABreakpointAll(CommandInterpreter &interpreter)
        : CommandObjectParsed(interpreter, "hsa kernel breakpoint all",
                              "Automatically sets a breakpoint on all hsa kernels that are or will be loaded.\n"
                              "Disabling option means breakpoints will no longer be set on any kernels loaded in the future, "
                              "but does not remove currently set breakpoints.",
                              "hsa kernel breakpoint all <enable/disable>",
                              eCommandRequiresProcess | eCommandProcessMustBeLaunched | eCommandProcessMustBePaused)
    {
    }

    ~CommandObjectHSABreakpointAll() override = default;

    bool
    DoExecute(Args &command, CommandReturnObject &result) override
    {
        const size_t argc = command.GetArgumentCount();
        if (argc != 1)
        {
            result.AppendErrorWithFormat("'%s' takes 1 argument of 'enable' or 'disable'", m_cmd_name.c_str());
            result.SetStatus(eReturnStatusFailed);
            return false;
        }

        //TODO the HSA runtime is registered for an arbitrary language. It probably shouldn't be a LanguageRuntime plugin
        HSARuntime *runtime =
          static_cast<HSARuntime *>(m_exe_ctx.GetProcessPtr()->GetLanguageRuntime(eLanguageTypeObjC));

        bool do_break = false;
        const char* argument = command.GetArgumentAtIndex(0);
        if (strcmp(argument, "enable") == 0)
        {
            do_break = true;
            result.AppendMessage("Breakpoints will be set on all kernels.");
        }
        else if (strcmp(argument, "disable") == 0)
        {
            do_break = false;
            result.AppendMessage("Breakpoints will not be set on any new kernels.");
        }
        else
        {
            result.AppendErrorWithFormat("Argument must be either 'enable' or 'disable'");
            result.SetStatus(eReturnStatusFailed);
            return false;
        }

        runtime->SetBreakAllKernels(do_break);

        result.SetStatus(eReturnStatusSuccessFinishResult);
        return true;
    }
};


//-------------------------------------------------------------------------
// CommandObjectHSABreakpointSet
//-------------------------------------------------------------------------
#pragma mark Set

class CommandObjectHSABreakpointSet : public CommandObjectParsed
{
public:
    CommandObjectHSABreakpointSet (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "hsa breakpoint set",
                             "Set HSA breakpoints.",
                             NULL),
        m_options (interpreter)
    {
    }

    virtual
    ~CommandObjectHSABreakpointSet () {}

    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter),
            m_pc (0),
            //m_data_mode (HSADebugAPI::All),
            m_data_size (0),
            m_data_address (0),
            m_kernel_name ("")
        {
        }


        virtual
        ~CommandOptions () {}

        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;

            switch (short_option)
            {
                case 'a':
                {
                    ExecutionContext exe_ctx (m_interpreter.GetExecutionContext());
                    m_data_address = Args::StringToAddress(&exe_ctx, option_arg, LLDB_INVALID_ADDRESS, &error);
                }
                break;

                case 'l':
                {
                    ExecutionContext exe_ctx (m_interpreter.GetExecutionContext());
                    m_pc = Args::StringToAddress(&exe_ctx, option_arg, LLDB_INVALID_ADDRESS, &error);
                }
                break;

                case 's':
                {
                    m_data_size = StringConvert::ToUInt64(option_arg, UINT64_MAX);
                }
                break;

                case 'k':
                {
		  if (option_arg)
		    m_kernel_name = option_arg;
                }
		break;


		/*case 't':
                {
		  int32_t result;
		  result = Args::StringToOptionEnum (option_arg, data_breakpoint_enums, 2, error);
                    if (error.Success());
		    m_data_mode = (HSADebugAPI::DataBreakpointMode) result;
                }
                break;*/


                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }

            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_pc = 0;
            //m_data_mode = HSADebugAPI::Read;
            m_data_size = 0;
            m_data_address = 0;
            m_kernel_name = "";
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.
        static OptionDefinition g_option_table[];

        hsa_pc m_pc;
      //HSADebugAPI::DataBreakpointMode m_data_mode;
        uint64_t m_data_size;
        addr_t m_data_address;
        std::string m_kernel_name;
    };

protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
      auto target = m_exe_ctx.GetTargetSP();
      
      if (!target) {
          result.AppendError("Target needed");
          return false;
      }
	    
      if (m_options.m_pc != 0) {
	SearchFilterSP filter_sp (new SearchFilterForUnconstrainedSearches(target));
	BreakpointResolverSP resolver_sp (new HSABreakpointResolver(nullptr, ConstString(m_options.m_kernel_name), m_options.m_pc));
	target->CreateBreakpoint(filter_sp, resolver_sp, false, false, false);
      }
      else {
	SearchFilterSP filter_sp (new SearchFilterForUnconstrainedSearches(target));
	BreakpointResolverSP resolver_sp (new HSABreakpointResolver(nullptr, ConstString(m_options.m_kernel_name)));
	target->CreateBreakpoint(filter_sp, resolver_sp, false, false, false);
      }

      return true;
    }

private:
    CommandOptions m_options;
};

#pragma mark Set::CommandOptions
OptionDefinition
CommandObjectHSABreakpointSet::CommandOptions::g_option_table[] =
{
        { LLDB_OPT_SET_1, false, "data-address", 'a', OptionParser::eRequiredArgument,   NULL, NULL, 0, eArgTypeAddress,
          "Address of the data" },
        { LLDB_OPT_SET_1, false, "data-size", 's', OptionParser::eRequiredArgument,   NULL, NULL, 0, eArgTypeUnsignedInteger,
          "Size of the data" },
	//        { LLDB_OPT_SET_1, false, "data-breakpoint-type", 't', OptionParser::eRequiredArgument,   NULL, data_breakpoint_enums, 0, eArgTypeNone,
	//"Type of the data breakpoint" },

        { LLDB_OPT_SET_2, false, "hsail-line", 'l', OptionParser::eRequiredArgument,   NULL, NULL, 0, eArgTypeAddress,
          "HSAIL line to break on" },
        { LLDB_OPT_SET_2, false, "kernel", 'k', OptionParser::eOptionalArgument,   NULL, NULL, 0, eArgTypeFunctionName,
          "Sets a breakpoint on a given kernel or all kernels" },
        { 0, false, NULL, 0, 0, NULL, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectMultiwordHSABreakpoint
//-------------------------------------------------------------------------
#pragma mark Breakpoint

class CommandObjectMultiwordHSABreakpoint : public CommandObjectMultiword
{
public:
    CommandObjectMultiwordHSABreakpoint (CommandInterpreter &interpreter) :
        CommandObjectMultiword (interpreter,
                             "hsa breakpoint",
                             "A set of commands for operating on HSA breakpoints",
                             "hsa breakpoint <command> [<command-options>]")
    {

        CommandObjectSP set_command_object (new CommandObjectHSABreakpointSet (interpreter));
        CommandObjectSP all_command_object (new CommandObjectHSABreakpointAll (interpreter));

        set_command_object->SetCommandName ("hsa breakpoint set");
        all_command_object->SetCommandName ("hsa breakpoint all");

        LoadSubCommand ("set",       set_command_object);
        LoadSubCommand ("all",       all_command_object);
    }


    virtual
    ~CommandObjectMultiwordHSABreakpoint () {}
};


//-------------------------------------------------------------------------
// CommandObjectHSAVariableRead
//-------------------------------------------------------------------------
#pragma mark Set

class CommandObjectHSAVariableRead : public CommandObjectParsed
{
public:
    CommandObjectHSAVariableRead (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "hsa variable read",
                             "Read HSA variables.",
                             NULL),
        m_options (interpreter)
    {
    }

    virtual
    ~CommandObjectHSAVariableRead () {}

    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter),
            m_var_name (""),
            m_x (0),
            m_y (0),
            m_z (0)
        {
        }


        virtual
        ~CommandOptions () {}

        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;

            switch (short_option)
            {
            case 'v':
                m_var_name = option_arg; break;
            case 'x':
                m_x = StringConvert::ToUInt64(option_arg, UINT32_MAX, 0); break;
            case 'y':
                m_y = StringConvert::ToUInt64(option_arg, UINT32_MAX, 0); break;
            case 'z':
                m_z = StringConvert::ToUInt64(option_arg, UINT32_MAX, 0); break;
            default:
                error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                break;
            }

            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_var_name = "";
            m_x = 0;
            m_y = 0;
            m_z = 0;
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.
        static OptionDefinition g_option_table[];

        std::string m_var_name;
        size_t m_x;
        size_t m_y;
        size_t m_z;
    };

protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        auto target_sp = m_exe_ctx.GetTargetSP();
        auto& images = target_sp->GetImages();

        ModuleSP module_sp;
        for (unsigned i=0; i < images.GetSize(); ++i) {
            auto module = images.GetModuleAtIndex(i);
            if (module) {
                if (module->GetArchitecture().GetMachine() == llvm::Triple::hsail) {
                    module_sp = module;
                    break;
                }
            }
        }

        if (!module_sp) {
            result.AppendError("Currently unimplemented");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        auto sym_vendor = module_sp->GetSymbolVendor();
        if (sym_vendor) {
            auto sym_file = static_cast<SymbolFileAMDHSA*>(sym_vendor->GetSymbolFile());
            if (sym_file) {
                auto dbginfo = sym_file->GetDbgInfo();
                auto start_addr = m_exe_ctx.GetThreadSP()->GetRegisterContext()->GetPC();
                Stream &s = result.GetOutputStream();

                HwDbgInfo_err err;
                HwDbgInfo_variable var;
                if (m_options.m_var_name.size() > 0 && (m_options.m_var_name[0] == '%' || m_options.m_var_name[0] == '$')) {
                    var = hwdbginfo_low_level_variable(dbginfo, start_addr, true, m_options.m_var_name.c_str(), &err);
                }
                else {
                    var = hwdbginfo_variable(dbginfo, start_addr, true, m_options.m_var_name.c_str(), &err);
                }
                

                if (err != HWDBGINFO_E_SUCCESS) {
                    result.AppendError("Failed to get var from name: ");
                    result.AppendError(std::to_string(err).c_str());
                    result.SetStatus (eReturnStatusFailed);
                    return false;
                }

                std::vector<char> name (m_options.m_var_name.size() + 1);
                size_t name_len = 0;
                std::vector<char> type_name (50);
                size_t type_name_len = 0;
                size_t var_size = 0;
                HwDbgInfo_encoding enc;
                bool is_const = false;
                bool is_output = false;
                err = hwdbginfo_variable_data(var, name.size(), name.data(), &name_len, type_name.size(), type_name.data(), &type_name_len, &var_size, &enc, &is_const, &is_output);

                if (err != HWDBGINFO_E_SUCCESS) {
                    result.AppendError("Failed to get var info");
                    result.SetStatus (eReturnStatusFailed);
                    return false;
                }

                
                HwDbgInfo_locreg reg_type;
                unsigned int reg_num;
                bool deref_value;
                unsigned int offset;
                unsigned int resource;
                unsigned int isa_memory_region;
                unsigned int piece_offset;
                unsigned int piece_size;
                int const_add;
                err = hwdbginfo_variable_location(var, &reg_type, &reg_num, &deref_value, &offset, &resource, &isa_memory_region, &piece_offset, &piece_size, &const_add);

                
                lldb::addr_t func_addr = GetFunctionAddr(*target_sp, ConstString("GetVarValue"));


                auto& thread_list = m_exe_ctx.GetProcessPtr()->GetThreadList();
                auto old_thread = thread_list.GetSelectedThread();
                thread_list.SetSelectedThreadByIndexID(0);
                
                {
                    lldb::addr_t func_addr = GetFunctionAddr(*target_sp, ConstString("SetHsailThreadCmdInfo"));

                    auto wg_id = old_thread->GetID()-1;

                    std::stringstream expr;
                    expr << "((void* (*) (unsigned, unsigned, unsigned, unsigned)) 0x";
                    expr << std::hex << func_addr << std::dec;
                    expr << ")(" 
                         << wg_id << ','
                         << m_options.m_x << ','
                         << m_options.m_y << ','
                         << m_options.m_z << ')';

                    ValueObjectSP valobj;
                    auto exe_scope = m_exe_ctx.GetProcessPtr()->GetThreadList().GetThreadAtIndex(0, false)->GetStackFrameAtIndex(0).get();
                    EvaluateExpressionOptions options;
                    options.SetExecutionPolicy(eExecutionPolicyAlways);
                    auto res = target_sp->EvaluateExpression(expr.str().c_str(), exe_scope, valobj, options);

                    if (res != eExpressionCompleted) {
                        result.AppendError("Failed to read variable");
                        result.SetStatus (eReturnStatusFailed);
                        return false;
                    }
                }

                std::stringstream expr;
                expr << "((void* (*) (int, size_t, unsigned, bool, unsigned, unsigned, unsigned, unsigned, unsigned, int)) 0x";
                expr << std::hex << func_addr << std::dec;
                expr << ")(" 
                     << reg_type << ','
                     << var_size << ','
                     << reg_num << ','
                     << deref_value << ','
                     << offset << ','
                     << resource << ','
                     << isa_memory_region << ','
                     << piece_offset << ','
                     << piece_size << ','
                     << const_add << ')';

                ValueObjectSP valobj;
                auto exe_scope = m_exe_ctx.GetProcessPtr()->GetThreadList().GetThreadAtIndex(0, false)->GetStackFrameAtIndex(0).get();
                EvaluateExpressionOptions options;
                options.SetExecutionPolicy(eExecutionPolicyAlways);
                auto res = target_sp->EvaluateExpression(expr.str().c_str(), exe_scope, valobj, options);
                
                if (res != eExpressionCompleted) {
                    result.AppendError("Failed to read variable");
                    result.SetStatus (eReturnStatusFailed);
                    return false;
                }

                s << "(" << type_name.data() << ") ";
                if (valobj)
                    valobj->DumpPrintableRepresentation(s);
                s << '\n';

                thread_list.SetSelectedThreadByID(old_thread->GetID());
            }
        }

        return result.Succeeded();
    }
private:
    CommandOptions m_options;
};

#pragma mark Set::CommandOptions
OptionDefinition
CommandObjectHSAVariableRead::CommandOptions::g_option_table[] =
{
        { LLDB_OPT_SET_1, false, "name", 'v', OptionParser::eRequiredArgument,   NULL, NULL, 0, eArgTypeVarName,
          "Name of the variable to read" },
        { LLDB_OPT_SET_1, false, "wi-x", 'x', OptionParser::eOptionalArgument,   NULL, NULL, 0, eArgTypeUnsignedInteger,
          "X work item dimension" },
        { LLDB_OPT_SET_1, false, "wi-y", 'y', OptionParser::eOptionalArgument,   NULL, NULL, 0, eArgTypeUnsignedInteger,
          "Y work item dimension" },
        { LLDB_OPT_SET_1, false, "wi-z", 'z', OptionParser::eOptionalArgument,   NULL, NULL, 0, eArgTypeUnsignedInteger,
          "Z work item dimension" },
        { 0, false, NULL, 0, 0, NULL, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectMultiwordHSAVariable
//-------------------------------------------------------------------------
#pragma mark Variable

class CommandObjectMultiwordHSAVariable : public CommandObjectMultiword
{
public:
    CommandObjectMultiwordHSAVariable (CommandInterpreter &interpreter) :
        CommandObjectMultiword (interpreter,
                             "hsa variable",
                             "A set of commands for operating on HSA variables",
                             "hsa variable <command> [<command-options>]")
    {
        CommandObjectSP read_command_object (new CommandObjectHSAVariableRead (interpreter));

        read_command_object->SetCommandName ("hsa variable read");

        LoadSubCommand ("read",       read_command_object);
    }


    virtual
    ~CommandObjectMultiwordHSAVariable () {}
};

//-------------------------------------------------------------------------
// CommandObjectHSAMemoryRead
//-------------------------------------------------------------------------
#pragma mark Set

class CommandObjectHSAMemoryRead : public CommandObjectParsed
{
public:
    CommandObjectHSAMemoryRead (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "hsa memory read",
                             "Read HSA memory.",
                             NULL),
        m_options (interpreter)
    {
    }

    virtual
    ~CommandObjectHSAMemoryRead () {}

    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter),
            m_addr(0), m_n_bytes(32)
        {
        }


        virtual
        ~CommandOptions () {}

        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;

            switch (short_option)
            {
            case 'b':
                m_n_bytes = StringConvert::ToUInt64(option_arg, UINT32_MAX, 0); break;
            default:
                error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                break;
            }

            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_addr = 0;
            m_n_bytes = 32;
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.
        static OptionDefinition g_option_table[];

        lldb::addr_t m_addr;
        size_t m_n_bytes;
    };

protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        return CurrentlyUnimplemented(result);

        auto target_sp = m_exe_ctx.GetTargetSP();
        Stream &s = result.GetOutputStream();
        Error error;

        ExecutionContext exe_ctx (m_interpreter.GetExecutionContext());
        m_options.m_addr = Args::StringToAddress(&exe_ctx, command.GetArgumentAtIndex(0), LLDB_INVALID_ADDRESS, &error);


        auto& thread_list = m_exe_ctx.GetProcessPtr()->GetThreadList();
        auto old_thread = thread_list.GetSelectedThread();
        thread_list.SetSelectedThreadByIndexID(0);
     
        {   
            Error err;
            auto perms = ePermissionsWritable | ePermissionsReadable;
            auto addr = exe_ctx.GetProcessPtr()->AllocateMemory(m_options.m_n_bytes, perms, err);
            auto size_addr = exe_ctx.GetProcessPtr()->AllocateMemory(sizeof(size_t), perms, err);

            lldb::addr_t func_addr;
            SymbolContextList sc_list;
            target_sp->GetImages().FindFunctions(ConstString("GetPrivateMemory"), eFunctionNameTypeAuto, true, true, false, sc_list);
                
            if (sc_list.GetSize() > 0) {
                auto func = sc_list[0].function;
                if (func)  {
                    auto addr = func->GetAddressRange().GetBaseAddress();
                    func_addr = addr.GetLoadAddress(target_sp.get());
                }
            }

            std::stringstream expr;
            expr << "((bool (*) (HwDbgDim3, HwDbgDim3, size_t, size_t, size_t, void*, size_t*)) 0x";
            expr << std::hex << func_addr << std::dec;
            expr << ")(" 
                 << "HwDbgDim3{" << 0 << ',' << 0 << ',' << 0 << "},"
                 << "HwDbgDim3{" << 0 << ',' << 0 << ',' << 0 << "},"
                 << m_options.m_addr << ','
                 << 0 << ','
                 << m_options.m_n_bytes << ','
                 << "(void*)" << addr << ','
                 << "(size_t*)" << size_addr
                 << ')';

            ValueObjectSP valobj;
            auto exe_scope = m_exe_ctx.GetProcessPtr()->GetThreadList().GetThreadAtIndex(0, false)->GetStackFrameAtIndex(0).get();
            EvaluateExpressionOptions options;
            options.SetExecutionPolicy(eExecutionPolicyAlways);
            auto res = target_sp->EvaluateExpression(expr.str().c_str(), exe_scope, valobj, options);

            if (res != eExpressionCompleted) {
                result.AppendError("Failed to read memory");
                result.SetStatus (eReturnStatusFailed);
                return false;
            }


            std::vector<char> mem (m_options.m_n_bytes);
            auto n_read = m_exe_ctx.GetProcessPtr()->ReadMemory(addr, mem.data(), m_options.m_n_bytes, error);
            s << "Mem: \n";
            for (unsigned i = 0; i < n_read; ++i ) {
                s.PutHex8(mem[i]);
                s << ' ';
            }
                
        }

        thread_list.SetSelectedThreadByID(old_thread->GetID());

        return result.Succeeded();
    }
private:
    CommandOptions m_options;
};

#pragma mark Set::CommandOptions
OptionDefinition
CommandObjectHSAMemoryRead::CommandOptions::g_option_table[] =
{
        { LLDB_OPT_SET_1, false, "n-bytes", 'b', OptionParser::eRequiredArgument,   NULL, NULL, 0, eArgTypeVarName,
          "Number of bytes" },
        { 0, false, NULL, 0, 0, NULL, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectMultiwordHSAMemory
//-------------------------------------------------------------------------
#pragma mark Memory

class CommandObjectMultiwordHSAMemory : public CommandObjectMultiword
{
public:
    CommandObjectMultiwordHSAMemory (CommandInterpreter &interpreter) :
        CommandObjectMultiword (interpreter,
                             "hsa memory",
                             "A set of commands for operating on HSA memory",
                             "hsa memory <command> [<command-options>]")
    {
        CommandObjectSP read_command_object (new CommandObjectHSAMemoryRead (interpreter));

        read_command_object->SetCommandName ("hsa memory read");

        LoadSubCommand ("read",       read_command_object);
    }


    virtual
    ~CommandObjectMultiwordHSAMemory () {}
};

//-------------------------------------------------------------------------
// CommandObjectHSA
//-------------------------------------------------------------------------
#pragma mark HSA

CommandObjectHSA::CommandObjectHSA (CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "hsa",
                            "A set of commands for debugging HSA applications.",
                            "hsa <command> [<command-options>]")
{
    CommandObjectSP breakpoint_command_object (new CommandObjectMultiwordHSABreakpoint (interpreter));
    CommandObjectSP kernel_command_object (new CommandObjectMultiwordHSAKernel (interpreter));
    CommandObjectSP var_command_object (new CommandObjectMultiwordHSAVariable (interpreter));
    CommandObjectSP mem_command_object (new CommandObjectMultiwordHSAMemory (interpreter));
    

    breakpoint_command_object->SetCommandName ("hsa breakpoint");
    kernel_command_object->SetCommandName ("hsa kernel");
    var_command_object->SetCommandName ("hsa variable");
    mem_command_object->SetCommandName ("hsa memory");

    LoadSubCommand ("breakpoint",       breakpoint_command_object);
    LoadSubCommand ("kernel",           kernel_command_object);
    LoadSubCommand ("variable",         var_command_object);
    LoadSubCommand ("memory",         mem_command_object);
}

CommandObjectHSA::~CommandObjectHSA ()
{
}
