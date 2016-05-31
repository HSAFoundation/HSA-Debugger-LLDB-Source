//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief  Header file of pre- and post-dispatch callback
//==============================================================================
#ifndef _PREPOSTDISPATCH_CALLBACK_H_
#define _PREPOSTDISPATCH_CALLBACK_H_

namespace HwDbgAgent
{
// The pre and post dispatch callback prototypes
void PreDispatchCallback(const hsa_dispatch_callback_t* pRTParam, void* pUserArgs);

void PostDispatchCallback(const hsa_dispatch_callback_t* pRTParam, void* pUserArgs);

}
#endif

