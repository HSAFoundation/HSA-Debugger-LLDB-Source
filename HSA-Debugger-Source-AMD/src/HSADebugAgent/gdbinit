# Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.

# Enable this when debugging a gdb

add-auto-load-safe-path /


define hsailConfigure
# Enable logging, the file will be deleted by the high level script 
# if logging isnt desired as per the env variable
set logging on

#give a warning of the evil that has been done
echo HSAIL Configure Steps Done\n
echo ....Certain GDB signals have been changed\n

set pagination off
handle SIGUSR1 nostop pass noprint
handle SIGUSR2 stop pass print
set mi-async on
end

document hsailConfigure
This command configures GDB internals for debugging HSAIL kernels
end

define hsailReset

echo Undoing HSAIL configuration steps\n
echo HSAIL applications will not work anymore\n

handle SIGCHLD nostop pass noprint
handle SIGUSR1 stop pass print
set pagination on
set mi-async off
end

hsailConfigure

# Enable this to save the last gdb session's commands into ./gdb_history.
#shell rm ./gdb_history
#set history filename ./gdb_history
#set history save
