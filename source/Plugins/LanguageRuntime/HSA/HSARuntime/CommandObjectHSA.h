//===-- CommandObjectHSA.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CommandObjectHSA_h_
#define liblldb_CommandObjectHSA_h_

// C Includes
// C++ Includes

#include <utility>
#include <vector>

// Other libraries and framework includes
// Project includes
#include "lldb/Core/Address.h"
#include "lldb/Interpreter/CommandObjectMultiword.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Core/STLUtils.h"

namespace lldb_private {

//-------------------------------------------------------------------------
// CommandObjectHSA
//-------------------------------------------------------------------------

class CommandObjectHSA : public CommandObjectMultiword
{
public:
    CommandObjectHSA (CommandInterpreter &interpreter);

    virtual
    ~CommandObjectHSA ();
};

} // namespace lldb_private

#endif  // liblldb_CommandObjectHSA_h_
