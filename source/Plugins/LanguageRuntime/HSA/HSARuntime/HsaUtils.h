//===-- HsaUtils.h ----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_HsaUtils_h_
#define liblldb_HsaUtils_h_

#include "lldb/Core/Log.h"
#include "lldb/Core/Logging.h"

namespace lldb_private {
template <typename... Ts>
void LogMsg (const char* fmt, const Ts&... ts) {
    Log* log(GetLogIfAllCategoriesSet(LIBLLDB_LOG_LANGUAGE));

    if (log)
      log->Printf(fmt, ts...);
} 

template <typename... Ts>
void LogBkpt (const char* fmt, const Ts&... ts) {
    Log* log(GetLogIfAllCategoriesSet(LIBLLDB_LOG_LANGUAGE | LIBLLDB_LOG_BREAKPOINTS));

    if (log)
      log->Printf(fmt, ts...);
}
} // namespace lldb_private

#endif // liblldb_HsaUtilsPacket
