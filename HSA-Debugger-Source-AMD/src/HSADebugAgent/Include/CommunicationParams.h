//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief  Parameters used for communication of agent and gdb
//==============================================================================
#ifndef _COMMUNICATION_PARAMS_H_
#define _COMMUNICATION_PARAMS_H_

// Value for Fifo permissions, more research necessary to figure out right values
const int g_FIFO_PERMISSIONS = 0666;

// Random number for SHM Segment which communicates the DBE memory to GDB
const int g_DBEBINARY_SHMKEY = 1234;

const int g_WAVE_BUFFER_SHMKEY = 2222;

const int g_MOMENTARY_BP_BUFFER_SHMKEY = 1111;

const int g_ISASTREAM_SHMKEY = 4567;

const size_t g_MOMENTARY_BP_BUFFER_MAXSIZE = 1024 * 1024 * 20;

const size_t g_BINARY_BUFFER_MAXSIZE = 1024 * 1024 * 10;

const size_t g_WAVE_BUFFER_MAXSIZE = 1024 * 1024;

const size_t g_ISASTREAM_MAXSIZE = 1024 * 1024;

// The names of the Fifos - opened in GDB and the agent

// The FIFO written to by the agent and read by GDB (For things like bp statistics)
const char gs_AgentToGdbFifoName[]  = "fifo-agent-w-gdb-r";

// The FIFO written to by GDB and read by Agent (For things like create / delete bp command)
const char gs_GdbToAgentFifoName[]  = "fifo-gdb-w-agent-r";

#endif // _COMMUNICATIONPARMS_H
