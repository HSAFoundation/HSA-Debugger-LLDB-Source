//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
//
/// \author AMD Developer Tools
/// \file
/// \brief   Functions to initialize communication of the agent with gdb
//==============================================================================
// Headers for signals
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

// Headers for shared mem
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <errno.h>

// Regular headers
#include <stdlib.h>
#include <fcntl.h>
#include <cassert>
#include <iostream>

#include "AgentLogging.h"
#include "AgentUtils.h"
#include "CommunicationControl.h"
#include "CommunicationParams.h"

/// The descriptor for the read end of the fifo
/// The constant could be moved to the CommunicationParams header but it is not since
/// this is only used by the agent and its better not to let gdb see the read end's descriptor
/// \todo Wrap all this into a class. Will be done once both fifos work
static int gs_FIFO_READ_DESC = 0;
static int gs_FIFO_WRITE_DESC = 0;


/// This function creates both the communication FIFOs that will be used
/// \todo make FIFO naming randomly generated, use some combination of date and time
/// FIFO naming is shown below:
/// Data flow Direction     Filename
/// agent <-- gdb           fifo-gdb-w-agent-r
/// agent --> gdb           fifo-agent-w-gdb-r
HsailAgentStatus CreateCommunicationFifos()
{
    int status = mkfifo(gs_GdbToAgentFifoName, g_FIFO_PERMISSIONS);
    int errno_value = errno;

    if (status != 0)
    {
        if (errno_value == EEXIST)
        {
            AGENT_LOG("FIFO " <<  gs_GdbToAgentFifoName << " already exists");
        }
        else
        {
            AGENT_ERROR("Error creating FIFO " <<  gs_GdbToAgentFifoName);
            return HSAIL_AGENT_STATUS_FAILURE;
        }
    }

    status = mkfifo(gs_AgentToGdbFifoName, g_FIFO_PERMISSIONS);
    errno_value = errno;

    if (status != 0)
    {
        if (errno_value == EEXIST)
        {
            AGENT_LOG("FIFO " <<  gs_AgentToGdbFifoName << " already exists");
        }
        else
        {
            AGENT_ERROR("Error creating FIFO " <<  gs_AgentToGdbFifoName);
            return HSAIL_AGENT_STATUS_FAILURE;
        }

    }

    return HSAIL_AGENT_STATUS_SUCCESS;
}

/// Initialize the read end of the fifo
/// This function is called directly after the shared memory creation
/// The agent's OnLoad function is responsible for delaying calling this function
/// until the shared memory initialization phase is done
HsailAgentStatus InitFifoReadEnd()
{

    AGENT_LOG("Opening FIFO GDB  ==> Agent");

    // Open fifo,  make this blocking ?
    // This was the old call fd = open("fifo", O_RDONLY|O_NONBLOCK);
    // The Agent will read this fifo for things to do from GDB
    gs_FIFO_READ_DESC = open(gs_GdbToAgentFifoName, O_RDONLY | O_NONBLOCK);

    if (gs_FIFO_READ_DESC <= 0)
    {
        AGENT_ERROR("Error opening in Read Fifo " <<
                    "DBE: GDB-> Agent Fifo ID: " << gs_FIFO_READ_DESC);
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    AGENT_LOG("Finished Opening FIFO GDB  ==> Agent");

    return HSAIL_AGENT_STATUS_SUCCESS ;
}

/// Return the descriptor of the fifo's read end
int GetFifoReadEnd()
{
    return gs_FIFO_READ_DESC;
}

/// Return the descriptor of the fifo's write end
int GetFifoWriteEnd()
{
    return gs_FIFO_WRITE_DESC;
}


HsailAgentStatus InitFifoWriteEnd()
{
    gs_FIFO_WRITE_DESC = -1;

    AGENT_LOG("Opening FIFO GDB  <== Agent");
    gs_FIFO_WRITE_DESC = open(gs_AgentToGdbFifoName, O_WRONLY);

    if (gs_FIFO_WRITE_DESC <= 0)
    {
        AGENT_ERROR("Error opening write fifo:" << "WriteFifo ID " << gs_FIFO_WRITE_DESC);
        return HSAIL_AGENT_STATUS_FAILURE;
    }

    AGENT_LOG("Finished Opening FIFO GDB  <== Agent");
    return HSAIL_AGENT_STATUS_SUCCESS ;
}


// Shared memory based initialization routines
// The code and functions below were used when we did the shared memory based
// initialization with gdb7.7.
//
// We dont use these routines anymore, but they have been left here to teach
// us about a way to do synchronization using shared mem, if we need to later.


// This static constants denotes the memory location where GDB and the DBE will write to
static const int g_GDB_OFFSET = 1;
static const int g_DBE_OFFSET = 2;

/// The number of iterations in the waiting loops
static const int g_TIMEOUT = 100;


// This function is not used presently in the GDB codebase
// The shared memory is not created, only its presence is checked
void CheckSharedMem(const key_t shmkey, const int maxShmSize)
{
    //Check if the shared memory has been created
    key_t key = shmkey;
    int shmid;
    int count = 0;

    do
    {
        shmid = shmget(key, maxShmSize, 0666);
        AGENT_LOG("GDB: Waiting for Shared Memory");
        sleep(1);
        count++;
    }
    while (shmid < 0 && count < g_TIMEOUT);

}

// We can delete the share memory buffer after both phases of the HSAIL
// initialization are complete
// The segment will only actually be destroyed after the last process detaches
HsailAgentStatus AgentFreeSharedMemBuffer(const key_t shmkey, const int maxShmSize)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;
    key_t key = shmkey;
    int shmid = shmget(key, maxShmSize, 0666);

    // We do the get pointer attach and detach routine. It is unclear to me
    // if shmget is the same as attaching to a shared memory segment
    //
    // So, to be safe we get the shmid, get the pointer once again and then
    // use the pointer explicitly detach before we do the SHM removal

    // Get the pointer to the shmem segment,
    void* pShm = shmat(shmid, nullptr, 0);

    if (pShm == (int*) - 1)
    {
        AGENT_ERROR("FreeSharedMemoryBuffer: Error with shmat");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }

    // Detach so we can be sure
    if (shmdt(pShm) == -1)
    {
        AGENT_ERROR("FreeSharedMemoryBuffer: shmdt failed\n");
        status = HSAIL_AGENT_STATUS_FAILURE;
        return status;
    }


    int shmStatus = shmctl(shmid, IPC_RMID, nullptr);
    int errno_value = errno;

    if (0  > shmStatus)
    {
        AGENT_ERROR("FreeSharedMemoryBuffer: Error: shmctl IPC_RMID returned " <<
                    shmStatus <<
                    "\t" << "ErrorNo" << errno_value);

        status = HSAIL_AGENT_STATUS_FAILURE;
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

void* AgentMapSharedMemBuffer(const key_t shmkey, const int maxShmSize)
{
    // Send the waves to gdb
    key_t key = shmkey;
    int shmid = shmget(key, maxShmSize , 0666);

    if (shmid  < 0)
    {
        AGENT_ERROR("AgentMapSharedMemBuffer: Error with shmget\n");
        return nullptr;
    }

    // Get the pointer to the shmem segment
    void* pShm = nullptr;
    pShm = shmat(shmid, nullptr, 0);

    if (pShm == (int*) - 1)
    {
        AGENT_ERROR("AgentMapSharedMemBuffer: Error with shmat\n");

        return nullptr;
    }

    return pShm;
}


HsailAgentStatus AgentUnMapSharedMemBuffer(void* pShm)
{
    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (pShm == nullptr)
    {
        AGENT_ERROR("AgentUnMapSharedMemBuffer: invalid input\n");
    }

    // Detach shared memory
    if (shmdt(pShm) == -1)
    {
        AGENT_ERROR("AgentUnMapSharedMemBuffer: shmdt failed\n");
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

/// Creates a shared memory buffer, using a known key( g_SHMKEY).
/// This shared memory buffer is used in the initialization of the HSAIL debugger
HsailAgentStatus AgentAllocSharedMemBuffer(const key_t shmkey, const int maxShmSize)
{
    key_t key = shmkey;
    int shmid = shmget(key, maxShmSize, IPC_CREAT | 0666);

    HsailAgentStatus status = HSAIL_AGENT_STATUS_FAILURE;

    if (shmid  < 0)
    {
        AGENT_ERROR("Error with shmget\n");
        AgentFatalExit();
    }

    void* pShm = (int*)shmat(shmid, nullptr, 0);

    if (pShm == (int*) - 1)
    {
        AGENT_ERROR("Error with shmat\n");
        return status;
    }

    AGENT_LOG("AllocSharedMemBuffer: Test with mapping shared memory");

    if (shmdt(pShm) == -1)
    {
        AGENT_ERROR("AllocateMomentaryBPBuffer: shmdt failed\n");
    }
    else
    {
        status = HSAIL_AGENT_STATUS_SUCCESS;
    }

    return status;
}

// Waits for the shared memory update from GDB
// This function will be called after an interrupt has been intercepted by GDB
// Returns 1 if GDB has written into shared memory, 0 otherwise
HsailAgentStatus WaitForSharedMemoryUpdate(const key_t shmkey, const int maxShmSize)
{
    int shmid;
    int count = 0;

    AGENT_LOG("Acquire Shared Memory");

    do
    {
        shmid = shmget(shmkey, maxShmSize, 0666);
        sleep(1);
        count++;

    }
    while (shmid < 0 && count <= g_TIMEOUT);

    int* pShm = (int*)shmat(shmid, nullptr, 0);
    // We now have the SHM
    count = 0;
    AGENT_LOG(" Waiting for GDB Update");

    if (pShm[g_GDB_OFFSET] == HSAIL_SIGNAL_GDB_READY)
    {
        AGENT_LOG("GDB Now Ready");
        return HSAIL_AGENT_STATUS_SUCCESS;
    }
    else
    {
        return HSAIL_AGENT_STATUS_FAILURE;
        AGENT_ERROR("DBE: Timeout error\n");
    }

    return HSAIL_AGENT_STATUS_FAILURE;
}
