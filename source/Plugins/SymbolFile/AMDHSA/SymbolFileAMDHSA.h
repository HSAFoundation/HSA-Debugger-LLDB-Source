//===-- SymbolFileAMDHSA.h --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef SymbolFileAMDHSA_SymbolFileAMDHSA_h_
#define SymbolFileAMDHSA_SymbolFileAMDHSA_h_

// C Includes
// C++ Includes
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <vector>

// Other libraries and framework includes
#include "lldb/lldb-private.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Host/FileSpec.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/SymbolContext.h"

// Project includes
#include "Plugins/SymbolFile/DWARF/SymbolFileDWARF.h"
#include "FacilitiesInterface.h"

class SymbolFileAMDHSA : public lldb_private::SymbolFile, public lldb_private::UserID
{
public:
    //------------------------------------------------------------------
    // Static Functions
    //------------------------------------------------------------------
    static void
    Initialize();

    static void
    Terminate();

    static lldb_private::ConstString
    GetPluginNameStatic();

    static const char *
    GetPluginDescriptionStatic();

    static lldb_private::SymbolFile*
    CreateInstance (lldb_private::ObjectFile* obj_file);

    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------

    SymbolFileAMDHSA(lldb_private::ObjectFile* ofile);

    ~SymbolFileAMDHSA() override;

    uint32_t
    CalculateAbilities () override;

    void
    InitializeObject() override;

    HwDbgInfo_debug GetDbgInfo() {
        return m_dbginfo;
    }

    //------------------------------------------------------------------
    // Compile Unit function calls
    //------------------------------------------------------------------

    uint32_t
    GetNumCompileUnits() override;

    lldb::CompUnitSP
    ParseCompileUnitAtIndex(uint32_t index) override;

    lldb::LanguageType
    ParseCompileUnitLanguage (const lldb_private::SymbolContext& sc) override;

    size_t
    ParseCompileUnitFunctions (const lldb_private::SymbolContext& sc) override;

    bool
    ParseCompileUnitLineTable (const lldb_private::SymbolContext& sc) override;

    bool
    ParseCompileUnitSupportFiles (const lldb_private::SymbolContext& sc,
                                  lldb_private::FileSpecList& support_files) override;

    bool
    ParseImportedModules (const lldb_private::SymbolContext &sc,
                          std::vector<lldb_private::ConstString> &imported_modules) override;

    size_t
    ParseFunctionBlocks (const lldb_private::SymbolContext& sc) override;

    size_t
    ParseTypes (const lldb_private::SymbolContext& sc) override;

    size_t
    ParseVariablesForContext (const lldb_private::SymbolContext& sc) override;

    bool
    ParseCompileUnitDebugMacros (const lldb_private::SymbolContext& sc) override;

    lldb_private::Type *
    ResolveTypeUID(lldb::user_id_t type_uid) override;

    bool
    CompleteType (lldb_private::CompilerType& compiler_type) override;

    lldb_private::CompilerDecl
    GetDeclForUID (lldb::user_id_t uid) override;

    lldb_private::CompilerDeclContext
    GetDeclContextForUID (lldb::user_id_t uid) override;

    lldb_private::CompilerDeclContext
    GetDeclContextContainingUID (lldb::user_id_t uid) override;

    void
    ParseDeclsForContext (lldb_private::CompilerDeclContext decl_ctx) override;
    

    uint32_t
    ResolveSymbolContext (const lldb_private::Address& so_addr,
                          uint32_t resolve_scope,
                          lldb_private::SymbolContext& sc) override;

    uint32_t
    ResolveSymbolContext (const lldb_private::FileSpec& file_spec,
                          uint32_t line,
                          bool check_inlines,
                          uint32_t resolve_scope,
                          lldb_private::SymbolContextList& sc_list) override;

    uint32_t
    FindGlobalVariables (const lldb_private::ConstString &name,
                         const lldb_private::CompilerDeclContext *parent_decl_ctx,
                         bool append,
                         uint32_t max_matches,
                         lldb_private::VariableList& variables) override;

    uint32_t
    FindGlobalVariables (const lldb_private::RegularExpression& regex,
                         bool append,
                         uint32_t max_matches,
                         lldb_private::VariableList& variables) override;

    uint32_t
    FindFunctions (const lldb_private::ConstString &name,
                   const lldb_private::CompilerDeclContext *parent_decl_ctx,
                   uint32_t name_type_mask,
                   bool include_inlines,
                   bool append,
                   lldb_private::SymbolContextList& sc_list) override;

    uint32_t
    FindFunctions (const lldb_private::RegularExpression& regex,
                   bool include_inlines,
                   bool append,
                   lldb_private::SymbolContextList& sc_list) override;

    uint32_t
    FindTypes (const lldb_private::SymbolContext& sc,
               const lldb_private::ConstString &name,
               const lldb_private::CompilerDeclContext *parent_decl_ctx,
               bool append,
               uint32_t max_matches,
               lldb_private::TypeMap& types) override;

    lldb_private::TypeList *
    GetTypeList () override;

    size_t
    GetTypes (lldb_private::SymbolContextScope *sc_scope,
              uint32_t type_mask,
              lldb_private::TypeList &type_list) override;

    //------------------------------------------------------------------
    // PluginInterface protocol
    //------------------------------------------------------------------
    lldb_private::ConstString
    GetPluginName() override;

    uint32_t
    GetPluginVersion() override;

    std::vector<HwDbgInfo_addr>
    GetStepOutAddresses (HwDbgInfo_addr start_addr);                      

    std::vector<HwDbgInfo_addr>
    GetStepOverAddresses (HwDbgInfo_addr start_addr);

    lldb_private::ConstString
    GetKernelName();

private:
    std::vector<HwDbgInfo_addr>
    GetStepAddresses (HwDbgInfo_addr start_addr, bool step_out);

    SymbolFileDWARF m_dwarf_symbols;
    HwDbgInfo_debug m_dbginfo;
    lldb_private::FileSpec m_source_file_spec;
};

#endif  // SymbolFileAMDHSA_SymbolFileAMDHSA_h_
