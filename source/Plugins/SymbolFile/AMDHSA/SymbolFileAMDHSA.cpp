//===-- SymbolFileAMDHSA.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolFileAMDHSA.h"

// Other libraries and framework includes
#include "lldb/Core/ArchSpec.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Section.h"
#include "lldb/Host/File.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/CompileUnit.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/FileSystem.h"

using namespace lldb;
using namespace lldb_private;

void
SymbolFileAMDHSA::Initialize()
{
    PluginManager::RegisterPlugin (GetPluginNameStatic(),
                                   GetPluginDescriptionStatic(),
                                   CreateInstance);
}

void
SymbolFileAMDHSA::Terminate()
{
    PluginManager::UnregisterPlugin (CreateInstance);
}

ConstString
SymbolFileAMDHSA::GetPluginNameStatic()
{
    static ConstString g_name("amdhsa");
    return g_name;
}

const char *
SymbolFileAMDHSA::GetPluginDescriptionStatic()
{
    return "AMD HSA debug symbol file reader.";
}

SymbolFile*
SymbolFileAMDHSA::CreateInstance (ObjectFile* obj_file)
{
    return new SymbolFileAMDHSA(obj_file);
}

SymbolFileAMDHSA::SymbolFileAMDHSA(ObjectFile* ofile)
    : SymbolFile (ofile),
      m_dwarf_symbols (ofile)
{
    DataExtractor de;
    m_obj_file->GetData(0,m_obj_file->GetByteSize(),de);
	
    HwDbgInfo_err err;

    m_dbginfo = hwdbginfo_init_with_hsa_1_0_binary((void*)de.GetDataStart(), m_obj_file->GetByteSize(), &err);

    const char* hsail;
    size_t hsail_len;
    err = hwdbginfo_get_hsail_text(m_dbginfo, &hsail, &hsail_len);

    llvm::SmallString<PATH_MAX> output_file_path{};
    int temp_fd;
    llvm::sys::fs::createTemporaryFile("hsa_source.%%%%%%", "", temp_fd, output_file_path);

    File tmp_file (temp_fd, true);
    tmp_file.Write(hsail, hsail_len);

    tmp_file.GetFileSpec(m_source_file_spec);
}

SymbolFileAMDHSA::~SymbolFileAMDHSA()
{
}

uint32_t
SymbolFileAMDHSA::CalculateAbilities ()
{
    ArchSpec as;
    m_obj_file->GetArchitecture(as);
    if (as.GetMachine() == llvm::Triple::hsail) {
        return kAllAbilities;
    }

    return 0;
}

void
SymbolFileAMDHSA::InitializeObject()
{
}

uint32_t
SymbolFileAMDHSA::GetNumCompileUnits()
{
    return m_dwarf_symbols.GetNumCompileUnits();
}

lldb::CompUnitSP
SymbolFileAMDHSA::ParseCompileUnitAtIndex(uint32_t index)
{
    return m_dwarf_symbols.ParseCompileUnitAtIndex(index);
}

lldb::LanguageType
SymbolFileAMDHSA::ParseCompileUnitLanguage (const SymbolContext& sc)
{
    return m_dwarf_symbols.ParseCompileUnitLanguage(sc);
}

size_t
SymbolFileAMDHSA::ParseCompileUnitFunctions (const SymbolContext& sc)
{
    return m_dwarf_symbols.ParseCompileUnitFunctions(sc);
}

bool
SymbolFileAMDHSA::ParseCompileUnitLineTable (const SymbolContext& sc)
{
    return true;
}

bool
SymbolFileAMDHSA::ParseCompileUnitDebugMacros (const SymbolContext& sc)
{
    return true;
}


bool
SymbolFileAMDHSA::ParseCompileUnitSupportFiles (const SymbolContext& sc,
                                                FileSpecList& support_files)
{
    return true;
}

bool
SymbolFileAMDHSA::ParseImportedModules (const SymbolContext &sc,
                                        std::vector<ConstString> &imported_modules)
{
    return true;
}

size_t
SymbolFileAMDHSA::ParseFunctionBlocks (const SymbolContext& sc)
{
    return true;
}

size_t
SymbolFileAMDHSA::ParseTypes (const SymbolContext& sc)
{
    return true;
}

size_t
SymbolFileAMDHSA::ParseVariablesForContext (const SymbolContext& sc)
{
    return true;
}

Type *
SymbolFileAMDHSA::ResolveTypeUID(lldb::user_id_t type_uid)
{
    return nullptr;
}

bool
SymbolFileAMDHSA::CompleteType (CompilerType& compiler_type)
{
    return false;
}

CompilerDecl
SymbolFileAMDHSA::GetDeclForUID (lldb::user_id_t uid)
{
    return m_dwarf_symbols.GetDeclForUID(uid);
}

CompilerDeclContext
SymbolFileAMDHSA::GetDeclContextForUID (lldb::user_id_t uid)
{
    return m_dwarf_symbols.GetDeclContextForUID(uid);
}

CompilerDeclContext
SymbolFileAMDHSA::GetDeclContextContainingUID (lldb::user_id_t uid)
{
    return m_dwarf_symbols.GetDeclContextContainingUID(uid);
}

void
SymbolFileAMDHSA::ParseDeclsForContext (CompilerDeclContext decl_ctx)
{
    return;
}
    

uint32_t
SymbolFileAMDHSA::ResolveSymbolContext (const Address& so_addr,
                                        uint32_t resolve_scope,
                                        SymbolContext& sc)
{
    uint32_t resolved = 0;

    sc.module_sp = m_obj_file->GetModule();
    resolved |= eSymbolContextModule;

    HwDbgInfo_addr addr;
    auto err = hwdbginfo_nearest_mapped_addr(m_dbginfo, so_addr.GetFileAddress(), &addr);

    if (err != HWDBGINFO_E_SUCCESS) {
        return resolved;
    }

    HwDbgInfo_code_location loc;
    err = hwdbginfo_addr_to_line(m_dbginfo, addr, &loc);

    if (err != HWDBGINFO_E_SUCCESS) {
        return resolved;
    }

    HwDbgInfo_linenum line;
    std::vector<char> file (1024);
    size_t file_name_len;
    err = hwdbginfo_code_location_details(loc, &line, file.size(), file.data(), &file_name_len);
    std::string file_name (file.data());

    if (err != HWDBGINFO_E_SUCCESS) {
        return resolved;
    }

    auto section_list = m_obj_file->GetSectionList();
    
    if (!section_list)
        return resolved;

    auto text_section = section_list->FindSectionByName(ConstString(".hsatext"));
    if (!text_section)
        return resolved;

    if (err != HWDBGINFO_E_SUCCESS) {
        return resolved;
    }

    if (file_name == "/hsa::self().elf(\".source\"):text") {
        sc.line_entry = LineEntry(text_section, 0, 8, m_source_file_spec, line, 0, true, false, false, false, false);      }
    else {
        FileSpec fs (file_name, true);
        sc.line_entry = LineEntry(text_section, 0, 8, fs, line, 0, true, false, false, false, false);      
    }

    resolved |= eSymbolContextLineEntry;

    //TODO store this somewhere to avoid leaking
    sc.comp_unit = new CompileUnit(sc.module_sp, nullptr, m_source_file_spec, 0, eLanguageTypeExtHsail, false);
    resolved |= eSymbolContextCompUnit;

    return resolved;
}

uint32_t
SymbolFileAMDHSA::ResolveSymbolContext (const FileSpec& file_spec,
                                        uint32_t line,
                                        bool check_inlines,
                                        uint32_t resolve_scope,
                                        SymbolContextList& sc_list)
{
    return 0;
}

uint32_t
SymbolFileAMDHSA::FindGlobalVariables (const ConstString &name,
                     const CompilerDeclContext *parent_decl_ctx,
                     bool append,
                     uint32_t max_matches,
                     VariableList& variables)
{
    return 0;
}

uint32_t
SymbolFileAMDHSA::FindGlobalVariables (const RegularExpression& regex,
                                       bool append,
                                       uint32_t max_matches,
                                       VariableList& variables)
{
    return 0;
}

uint32_t
SymbolFileAMDHSA::FindFunctions (const ConstString &name,
                                 const CompilerDeclContext *parent_decl_ctx,
                                 uint32_t name_type_mask,
                                 bool include_inlines,
                                 bool append,
                                 SymbolContextList& sc_list)
{
    return 0;
}

uint32_t
SymbolFileAMDHSA::FindFunctions (const RegularExpression& regex,
               bool include_inlines,
               bool append,
               SymbolContextList& sc_list)
{
    return 0;
}

uint32_t
SymbolFileAMDHSA::FindTypes (const SymbolContext& sc,
           const ConstString &name,
           const CompilerDeclContext *parent_decl_ctx,
           bool append,
           uint32_t max_matches,
           TypeMap& types)
{
    return 0;
}

TypeList *
SymbolFileAMDHSA::GetTypeList ()
{
    return nullptr;
}

size_t
SymbolFileAMDHSA::GetTypes (SymbolContextScope *sc_scope,
                            uint32_t type_mask,
                            TypeList &type_list)
{
    return 0;
}

ConstString
SymbolFileAMDHSA::GetPluginName()
{
    return GetPluginNameStatic();
}

uint32_t
SymbolFileAMDHSA::GetPluginVersion()
{
    return 1;
}


std::vector<HwDbgInfo_addr>
SymbolFileAMDHSA::GetStepAddresses (HwDbgInfo_addr start_addr, bool step_out)
{
    HwDbgInfo_addr addr;
    auto err = hwdbginfo_nearest_mapped_addr(m_dbginfo, start_addr, &addr);

    if (err != HWDBGINFO_E_SUCCESS) {
        return {};
    }

    size_t n_addrs = 0;
    err = hwdbginfo_step_addresses(m_dbginfo, addr, step_out, 0, nullptr, &n_addrs);

    if (err != HWDBGINFO_E_SUCCESS) {
        return {};
    }

    std::vector<HwDbgInfo_addr> addrs (n_addrs);
    err = hwdbginfo_step_addresses(m_dbginfo, addr, step_out, n_addrs, addrs.data(), &n_addrs);

    if (err != HWDBGINFO_E_SUCCESS) {
        return {};
    }

    return addrs;
}

std::vector<HwDbgInfo_addr>
SymbolFileAMDHSA::GetStepOverAddresses (HwDbgInfo_addr start_addr)
{
    return GetStepAddresses(start_addr, false);
}

std::vector<HwDbgInfo_addr>
SymbolFileAMDHSA::GetStepOutAddresses (HwDbgInfo_addr start_addr)
{
    return GetStepAddresses(start_addr, true);
}
