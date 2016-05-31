#include <iostream>//===-- HSABreakpointResolver.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Triple.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Symbol/ObjectFile.h"
#include "HSABreakpointResolver.h"
#include "Plugins/SymbolFile/AMDHSA/FacilitiesInterface.h"

using namespace lldb;
using namespace lldb_private;

Searcher::CallbackReturn
HSABreakpointResolver::SearchCallback(SearchFilter &filter,
	       SymbolContext &context,
	       Address *addr,
	       bool containing)
{
  ModuleSP module_sp = context.module_sp;

  if (!module_sp)
    return Searcher::eCallbackReturnContinue;

  if (module_sp->GetArchitecture().GetMachine() != llvm::Triple::hsail) 
    return Searcher::eCallbackReturnContinue;    

  const size_t max_addrs = 10240;
  std::vector<HwDbgInfo_addr> addrs (max_addrs);
  size_t n_addrs = 0;

  auto obj_file = module_sp->GetObjectFile();
  if (!obj_file) {
    return Searcher::eCallbackReturnContinue;
  }

  DataExtractor de;
  obj_file->GetData(0,obj_file->GetByteSize(),de);
	
  HwDbgInfo_err err;

  //TODO use the symbol file instead
  auto dbginfo = hwdbginfo_init_with_hsa_1_0_binary((void*)de.GetDataStart(), obj_file->GetByteSize(), &err);

  if (m_type == Type::KernelEntry || m_type == Type::AllKernels) {
      err = hwdbginfo_all_mapped_addrs(dbginfo, max_addrs, addrs.data(), &n_addrs);
  } 
  else {
      auto loc = hwdbginfo_make_code_location(nullptr, m_line);
      if (loc == nullptr) {
          return Searcher::eCallbackReturnContinue;
      }

      HwDbgInfo_code_location resolvedLoc;
      auto err = hwdbginfo_nearest_mapped_line(dbginfo, loc, &resolvedLoc);
      hwdbginfo_release_code_locations(&loc, 1);
    
      if (err != HWDBGINFO_E_SUCCESS) { 
          return Searcher::eCallbackReturnContinue;
      }

      err = hwdbginfo_line_to_addrs(dbginfo, resolvedLoc, max_addrs, addrs.data(), nullptr);
      hwdbginfo_release_code_locations(&resolvedLoc, 1);
  }

  if (err != HWDBGINFO_E_SUCCESS) {
      return Searcher::eCallbackReturnContinue;
  } 
 
  auto start_addr = addrs[0];
  HwDbgInfo_frame_context frame;
  size_t n_frames;
  err = hwdbginfo_addr_call_stack(dbginfo, start_addr, 1, &frame, &n_frames);

  if (err != HWDBGINFO_E_SUCCESS) {
      return Searcher::eCallbackReturnContinue;
  } 

  HwDbgInfo_addr pc, fp, mp;
  HwDbgInfo_code_location loc;
  std::vector<char> func_name (100);
  size_t func_name_len;
  err = hwdbginfo_frame_context_details(frame, &pc, &fp, &mp, &loc, func_name.size(), func_name.data(), &func_name_len);
  
  if (m_type == Type::AllKernels || ConstString(func_name.data()) == m_kernel_name) {
      m_breakpoint->AddLocation(addrs[0]);
  }
  
  return Searcher::eCallbackReturnContinue;
}
