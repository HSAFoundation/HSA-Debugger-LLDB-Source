//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief Common header between hsail agent and gdb for setting up communication
//==============================================================================
// This header is shared with gdb
// It describes the communication packet
#ifndef _COMMUNICATION_CONTROL_H_
#define _COMMUNICATION_CONTROL_H_

#if defined (_WIN32)
    #define HSAIL_UNREFERENCED_PARAMETER( x ) ( x )
#elif defined (__linux__) || defined (__CYGWIN__)
    #define HSAIL_UNREFERENCED_PARAMETER( x )
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/shm.h>

typedef enum
{
    HSAIL_COMMAND_UNKNOWN,
    HSAIL_COMMAND_BEGIN_DEBUGGING,      // Start debugging
    HSAIL_COMMAND_KILL_ALL_WAVES,       // Kill all waves
    HSAIL_COMMAND_CREATE_BREAKPOINT,    // Set an HSAIL breakpoint
    HSAIL_COMMAND_DELETE_BREAKPOINT,    // Delete an HSAIL Breakpoint
    HSAIL_COMMAND_ENABLE_BREAKPOINT,    // Enable a HSAIL breakpoint
    HSAIL_COMMAND_DISABLE_BREAKPOINT,   // Disable a HSAIL breakpoint
    HSAIL_COMMAND_MOMENTARY_BREAKPOINT, // Set an HSAIL momentary breakpoint (which is automatically deleted)
    HSAIL_COMMAND_CONTINUE,             // Continue the inferior process
    HSAIL_COMMAND_SET_LOGGING,          // Configure the logging in the Agent
} HsailCommand;

typedef enum
{
    HSAIL_LOGGING_UNKNOWN,
    HSAIL_LOGGING_ENABLE_AGENT,        // Enable only the AgentLog function
    HSAIL_LOGGING_ENABLE_DBE,          // Enable the DBE logging
    HSAIL_LOGGING_ENABLE_ALL,          // Enable all logging
    HSAIL_LOGGING_DISABLE_ALL          // Disable all logging
} HsailLogCommand;

typedef enum
{
    HSAIL_NOTIFY_UNKNOWN,
    HSAIL_NOTIFY_BREAKPOINT_HIT,    // Used to update breakpoint statistics
    HSAIL_NOTIFY_NEW_BINARY,        // Used to pass a binary to HwDbgFacilities
    HSAIL_NOTIFY_AGENT_UNLOAD,      // Will be needed to let gdb know not to monitor this descriptor any more
    HSAIL_NOTIFY_BEGIN_DEBUGGING,   // Will be needed to let gdb know to switch focus to the device
    HSAIL_NOTIFY_END_DEBUGGING,     // Debugging has ended successfully
    HSAIL_NOTIFY_FOCUS_CHANGE,      // Change in focus workgroup and workitem
    HSAIL_NOTIFY_START_DEBUG_THREAD,// The debug thread has been started
    HSAIL_NOTIFY_PREDISPATCH_STATE, // Information about the predispatch callback
    HSAIL_NOTIFY_AGENT_ERROR,        // Some error from the agent or the DBE - let gdb know
    HSAIL_NOTIFY_KILL_COMPLETE       // Notification to let GDB know about kill finishing
} HsailNotification;

typedef enum
{
    HSAIL_SIGNAL_INVALID,
    HSAIL_SIGNAL_GDB_READY,
    HSAIL_SIGNAL_AGENT_READY

} HsailCommunicationSignal;

typedef enum
{
    HSAIL_PREDISPATCH_STATE_UNKNOWN,
    HSAIL_PREDISPATCH_ENTERED_PREDISPATCH,
    HSAIL_PREDISPATCH_LEFT_PREDISPATCH
} HsailPredispatchState;

typedef enum
{
    HSAIL_AGENT_STATUS_FAILURE, // A failure in the agent (The AgentErrorLog() will add more information)
    HSAIL_AGENT_STATUS_SUCCESS  // A success

} HsailAgentStatus;

#define AGENT_MAX_SOURCE_LINE_LEN 256

#define AGENT_MAX_FUNC_NAME_LEN 256

#define MAX_SHADER_BINARY_ELF_SYMBOL_LEN 256

#define HSAIL_MAX_REPORTABLE_BREAKPOINTS 64

// Shared definition of structures between Agent and GDB
// The following structures are mirrors of their versions in AMDGPUDebug.h
// This is necessary so that we don't have to include the DBE in gdb
typedef struct _HsailWaveDim3
{
    uint32_t x;
    uint32_t y;
    uint32_t z;

} HsailWaveDim3;

typedef struct _HsailNotificationPayload
{
    HsailNotification m_Notification;   // The type of notification
    union
    {
        // HSAIL_NOTIFY_BREAKPOINT_HIT
        // \todo this structure needs to be fixed such that we can report multiple
        // breakpoints and their hit count all in the same packet
        // Just using active waves for now
        struct
        {
            int m_breakpointId[HSAIL_MAX_REPORTABLE_BREAKPOINTS]; // The breakpoint ID (unused in agent for now)
            int m_hitCount[HSAIL_MAX_REPORTABLE_BREAKPOINTS];     // Number of times the breakpoint was hit (unused in agent for now)
            int m_numActiveWaves;                                 // The number of waves written to shared mem
        } BreakpointHit;

        // HSAIL_NOTIFY_NEW_BINARY
        struct
        {
            char m_llSymbolName[MAX_SHADER_BINARY_ELF_SYMBOL_LEN];  // The low level symbol
            char m_hlSymbolName[MAX_SHADER_BINARY_ELF_SYMBOL_LEN];  // The high level symbol
            char m_KernelName[AGENT_MAX_FUNC_NAME_LEN];    // The kernel name
            uint64_t m_binarySize;
            HsailWaveDim3 m_workGroupSize;
            HsailWaveDim3 m_gridSize;
        } BinaryNotification;

        // HSAIL_NOTIFY_PREDISPATCH_STATE
        struct
        {
            HsailPredispatchState m_predispatchState;
        } PredispatchNotification;

        // HSAIL_NOTIFY_BEGIN_DEBUGGING
        struct
        {
            bool setDeviceFocus;  // True if we need to switch focus to the device
        } BeginDebugNotification;

        // HSAIL_NOTIFY_END_DEBUGGING
        struct
        {
            bool hasDispatchCompleted;  // True if the dispatch has completed
        } EndDebugNotification;

        // HSAIL_NOTIFY_ERROR
        struct
        {
            int m_errorCode;    // The error code from the DBE or the agent
        } AgentErrorNotification;

        // HSAIL_NOTIFY_FOCUS
        struct
        {
            HsailWaveDim3 m_focusWorkGroup; // The focus workgroup
            HsailWaveDim3 m_focusWorkItem;  // The focus workitem
        } FocusChange;

        // HSAIL_NOTIFY_START_DEBUG_THREAD
        struct
        {
            int m_tid;  // The thread ID to let gdb know that this is a hsail helper thread
        } StartDebugThreadNotification;

        // HSAIL_NOTIFY_KILL_COMPLETE
        struct
        {
            bool killSuccessful;        // True if the kill command was successful
            bool isQuitCommandIssued;   // Used by handle_hsail_event if kill / quit command was originally issued
        } KillCompleteNotification;

    } payload;
} HsailNotificationPayload;


typedef struct _HsailMomentaryBP
{
    uint64_t m_pc;      // The PC for this momentary breakpoint
    int m_lineNum;      // The line number for this breakpoint
} HsailMomentaryBP;

typedef enum
{
    HSAIL_BREAKPOINT_CONDITION_UNKNOWN, // Unknown condition,
    HSAIL_BREAKPOINT_CONDITION_ANY,     // No condition, always returns true
    HSAIL_BREAKPOINT_CONDITION_EQUAL    // The workgroup and workitem are present in the waveinfo buffer
} HsailConditionCode;

typedef struct _HsailConditionPacket
{
    HsailConditionCode m_conditionCode;
    HsailWaveDim3 m_workitemID;
    HsailWaveDim3 m_workgroupID;

} HsailConditionPacket;

// \todo this structure needs to be improved with a Union, similar to the notification payload
typedef struct
{
    HsailCommand m_command;         // Command Type
    HsailLogCommand m_loggingInfo;  // Logging configuration
    int m_gdbBreakpointID;          // GDB breakpoint number (Will be sent while deleting a bp)
                                    // We could define m_gdbBreakpointID as GdbBkptNum but then typecasting
                                    // would have to be done in GDB - which could be more error prone
    uint64_t m_pc;                  // Program counter
    int m_hitCount;                 // The number of times the breakpoint was hit
    int m_lineNum;                  // The line number for kernel source breakpoints
    int m_numMomentaryBP;           // The number of momentary Breakpoints needed
    HsailConditionPacket m_conditionPacket;         // The condition info for this breakpoint
    char m_sourceLine[AGENT_MAX_SOURCE_LINE_LEN];   // The source line for kernel source breakpoints
    char m_kernelName[AGENT_MAX_FUNC_NAME_LEN];     // The kernel name for kernel function breakpoints
} HsailCommandPacket;

// the hardware wave address
typedef uint32_t HsailWaveAddress;

// the program counter (byte offset in the ISA binary)
typedef uint64_t HsailProgramCounter;

// We have filtered out the Databreakpointinfo for now
typedef struct _HsailAgentWaveInfo
{
    HsailWaveDim3           workGroupId;         /**< work-group id */
    HsailWaveDim3           workItemId[64];      /**< work-item id (local id within a work-group) */
    uint64_t                execMask;            /**< the execution mask of the work-items */
    HsailWaveAddress        waveAddress;         /**< the hw wave slot address (not unique for the dispatch) */
    HsailProgramCounter     pc;                  /**< the program counter for the wave */

} HsailAgentWaveInfo;


// A constant value to use when we send a packet that doesnt use the m_pc field
static const uint64_t HSAIL_ISA_PC_UNKOWN = (uint64_t)(-1);

// A macro for the signal used by the agent to signal GDB
#define AGENT_GDB_SIGNAL SIGCHLD


/// These functions are only used by the agent

/// Initialize the read end of the fifo
HsailAgentStatus InitFifoReadEnd();

/// Create the communication fifos
HsailAgentStatus CreateCommunicationFifos();

void CheckSharedMem(const key_t shmkey, const int maxShmSize);

/// Shared mem alloc utility
HsailAgentStatus AgentAllocSharedMemBuffer(const key_t shmkey, const int maxShmSize);

/// Shared mem free utility
HsailAgentStatus AgentFreeSharedMemBuffer(const key_t shmkey, const int maxShmSize);

/// Shared mem map utility
void* AgentMapSharedMemBuffer(const key_t shmkey, const int maxShmSize);

/// Shared mem unmap utility
HsailAgentStatus AgentUnMapSharedMemBuffer(void* pShm);

/// Used by the agent to wait for the shared memory update from GDB
/// \return 1 if the update is visible to the agent
HsailAgentStatus WaitForSharedMemoryUpdate(const key_t shmkey, const int maxShmSize);

/// Get descriptor of the read fifo
int GetFifoReadEnd();

/// Get descriptor of the write fifo
int GetFifoWriteEnd();

/// Initialize the Agent --> Fifo
HsailAgentStatus InitFifoWriteEnd();

#endif // _COMMUNICATIONCONTROL_H
