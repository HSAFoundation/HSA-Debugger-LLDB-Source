//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE.
//==============================================================================
/// \author Developer Tools
/// \file   CommunicationParms.h
/// \brief  Parameters used for communication of agent and gdb

#ifndef _COMMUNICATIONPARMS_H
#define _COMMUNICATIONPARMS_H

// Value for Fifo permissions, more research necessary to figure out right values
const int g_FIFO_PERMISSIONS = 0666;

// Random number for SHM Segment which communicates the DBE memory to GDB
const int g_DBEBINARY_SHMKEY = 1234;

const int g_WAVE_BUFFER_SHMKEY = 2222;

const int g_MOMENTARY_BP_BUFFER_SHMKEY = 1111;

const int g_ISASTREAM_SHMKEY = 4567;

const size_t g_MOMENTARY_BP_BUFFER_MAXSIZE = 1024*1024;

const size_t g_BINARY_BUFFER_MAXSIZE = 1024*1024*10;

const size_t g_WAVE_BUFFER_MAXSIZE = 1024*1024;

const size_t g_ISASTREAM_MAXSIZE = 1024*1024;

// The names of the Fifos - opened in GDB and the agent

// The FIFO written to by the agent and read by GDB (For things like bp statistics)
const char gs_AgentToGdbFifoName[]  = "fifo-agent-w-gdb-r";

// The FIFO written to by GDB and read by Agent (For things like create / delete bp command)
const char gs_GdbToAgentFifoName[]  = "fifo-gdb-w-agent-r";

#endif // _COMMUNICATIONPARMS_H
