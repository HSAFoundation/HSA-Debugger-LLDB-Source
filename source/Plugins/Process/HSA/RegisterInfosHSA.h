//===-- RegisterInfosHSA.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifdef DECLARE_REGISTER_INFOS_HSA_STRUCT

// C Includes
#include <stddef.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"

using namespace lldb;
using namespace lldb_private;

enum
{
  gpr_pc_hsa,
  gpr_fp_hsa,
  gpr_mp_hsa,
  k_num_registers
};

static RegisterInfo g_register_infos_hsa[] = {
// General purpose registers
//  NAME        ALT     SZ  OFFSET              ENCODING        FORMAT          EH_FRAME                DWARF               GENERIC                     PROCESS PLUGIN          LLDB NATIVE   VALUE REGS    INVALIDATE REGS
//  ======      ======= ==  =============       =============   ============    ===============         ===============     =========================   =====================   ============= ==========    ===============
{   "pc",       nullptr, 4, 0,      eEncodingUint,  eFormatHex,     { gpr_pc_hsa,           gpr_pc_hsa,           LLDB_REGNUM_GENERIC_PC,   LLDB_REGNUM_GENERIC_PC,    gpr_pc_hsa      },      nullptr,        nullptr},
{   "fp",       nullptr, 4, 0,      eEncodingUint,  eFormatHex,     { gpr_fp_hsa,           gpr_fp_hsa,           LLDB_REGNUM_GENERIC_FP,   LLDB_REGNUM_GENERIC_FP,    gpr_fp_hsa      },      nullptr,        nullptr},
{   "mp",       nullptr, 4, 0,      eEncodingUint,  eFormatHex,     { gpr_mp_hsa,           gpr_mp_hsa,           LLDB_REGNUM_GENERIC_ARG1,   LLDB_INVALID_REGNUM,    gpr_mp_hsa      },      nullptr,        nullptr}
};

#endif // DECLARE_REGISTER_INFOS_HSA_STRUCT
