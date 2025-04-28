# PennOS
PennOS is a UNIX-like operating system simulator built to run as a single process on a host OS. Its main features include a priority-based scheduler that handles threads using the spthread module, a custom FAT-like file system, and a shell with support for job control, system calls, and user-level programs. Though it doesn't boot on actual hardware, our PennOS simulates real OS abstractions like kernel vs. user land, process/thread management, and file handling.

## Members (Name and Pennkey)
- Dan Kim (pennkey: dankim1)
- Kevin Zhou (pennkey: kzhou1)
- Krystof Purtell (pennkey: krystofp)
- Richard Zhang (pennkey: richz)

## List of Submitted Files
- Makefile
- Companion Document
- doc/README.md
- src/fs/fat_routines.c
- src/fs/fat_routines.h
- src/fs/fs_helpers.c
- src/fs/fs_helpers.h
- src/fs/fs_kfuncs.c
- src/fs/fs_kfuncs.h
- src/fs/fs_syscalls.c
- src/fs/fs_syscalls.h
- src/kernel/kern_pcb.c
- src/kernel/kern_pcb.h
- src/kernel/kern_sys_calls.c
- src/kernel/kern_sys_calls.h
- src/kernel/logger.c
- src/kernel/logger.h
- src/kernel/scheduler.c
- src/kernel/scheduler.h
- src/kernel/signal.c
- src/kernel/signal.h
- src/kernel/stress.c
- src/kernel/stress.h
- src/lib/pennos-errno.c
- src/lib/pennos-errno.h
- src/lib/spthread.c
- src/lib/spthread.h
- src/lib/Vec.c
- src/lib/Vec.h
- src/shell/builtins.c
- src/shell/builtins.h
- src/shell/Job.h
- src/shell/parser.c
- src/shell/parser.h
- src/shell/shell_built_ins.c
- src/shell/shell_built_ins.h
- src/shell/shell.c
- src/shell/shell.h
- src/pennfat.c
- src/pennos.c

## Extra Credit Implemented
- Compaction of directory files (extra credit 1)

## Compilation Instructions
- `make` or `make all`: create executables of mains in src/
- `make tests`: create executables of test mains in tests/
- `make info`: list which files are set as main, execs, etc.
- `make format`: auto format main, test main, src, and header files
- `make clean`: delete *.o and executable files

## How to Run
- Run PennFAT standalone
```
./bin/pennfat
```
- Run PennOS
```
./bin/pennos [filesystem] [logfile]
```

## Overview of Work Accomplished

### PennFAT File System
The standalone PennFAT provides an interface for creating, mounting, and unmounting a filesystem as well as running various routines such as `cp`, `cat`, `ls`, `touch`, `rm`, `mv`, and `chmod`. 
- **The standalone PennFAT**
    - Runs as a continuous loop, prompting user for input, parsing the arguments, and executing the corresponding command.
    - Implements signal handling to properly respond to Ctrl-C and Ctrl-Z signals.
- **Filesystem regions**
    - *FAT region*: Stores the File Allocation Table which tracks block allocation and file chains.
    - *Data region*: Contains the root directory and all file data.
    - The first FAT entry stores filesystem metadata (block size and FAT size).
    - Block 1 is always reserved for the root directory. Note, files and directories can span multiple blocks.
    - Allocation and mapping of fat region is taken care of in `mkfs` and `mount`
- **Core Data Structures**
    - *Directory entry structure*: Stores file metadata including name, size, first block, type, permissions, and modification time.
    - *File descriptor entry structure*: Holds metadata about each file descriptor in the system-wide file descriptor table. Tracks open file state including position, access mode, and reference counts.
- **File Descriptor Management**
    - Maintains a system-wide file descriptor table to track all open files.
    - Reserves standard file descriptors (0, 1, 2) for stdin, stdout, and stderr.
    - Enforces access restrictions (only one process can write to a file at a time).
    - Maintains proper open file reference counting.
- **Block Allocation and Directory Management**
    - Adds logic for finding files in a root directory and writing file entries to the filesystem.
    - Allocates new blocks as directories or files grow.
    - De-allocates old blocks as files or directories are truncated or deleted.
- **Abstraction**
    - System call functions (s_ functions) are wrappers around kernel functions to provide an interface for user programs.
    - Kernel-level functions (k_ functions) implement core filesystem operations such as k_open, k_close, k_read, k_write, k_lseek, k_unlink, and k_ls.
    - Process control blocks maintain per-process file descriptor tables.
- **Summary of Core Features**
    - *Basic file operations*: open, read, write, close, unlink, lseek
    - *File manipulation utilities*: cat, ls, touch, mv, cp, rm
    - *Filesystem management*: mkfs, mount, unmount

### Kernel
The PennOS kernel provides a thread scheduler and process management system that simulates a real operating system's functionality while running as a single process on the host OS.
- **Process Management**
    - *Process Control Block (PCB)*: Tracks a process state including the thread handle, pid, parent pid, children processes, priority level, process state, signals, and file descriptors, sleeping status, and wake up time.
    - Creates and deletes processes, while also managing resources.
    - Inherits properties of parent process and tracks parent-child relationships.
    - Reparents orphaned child processes to INIT.
    - Handles file desciptors.
    - Properly schedules new processes in corresponding queues.
    - Manages and reaps zombie children.
- **Signal Handling and Process States**
    - Defines 3 signals: P_SIGSTOP, P_SIGCONT, and P_SIGTERM
    - Supports macros P_WIFEXITED, P_WIFSTOPPED, P_WIFSIGNALED based on status definitions
- **System Calls**
    - *Process creation*: s_spawn and child process spawning
    - *Process control*: s_waitpid, s_kill, s_exit
    - *Scheduler interaction*: s_nice, s_sleep
- **Priority-based Scheduler**
    - Implements three priority levels (0, 1, 2) `zero_priority_queue`, `one_priority_queue`, and `two_priority_queue`.
    - Uses round-robin scheduling within each priority level.
    - Supports time-sliced execution with 100ms quanta.
    - Handles blocked, stopped, and sleeping processes.
- **Clock Ticks and Timing**
    - Implemented system clock implementation using SIGALRM
- **Idling**
    - Manages CPU usage when no processes are runnable
    - Implemented sigsuspend
- **Logging**
    - Implements event logging for debugging and verification with timestamps

### Shell
The PennOS shell provides a user interface to interact with the simulated operating system, offering a set of built-in commands and job control features.
- CLI
    - Prompts user for command and parses arguments
    - Supports redirection
    - Supports fg/bg jobs
    - Handles signals for user interrupts
- Built-in commands
    - For files: cat, ls, touch, mv, cp, rm, chmod
    - For processes: ps, kill, nice, nice_pid
    - For jobs: bg, fg, jobs
    - For utilities: sleep, busy, echo, man
    - For testing utils: zombify, orphanify
    - System control: logout
- Process hierarchy
    - Shell as parent of user commands
    - Child process spawning and monitoring
    - Zombie reaping and cleanup
- Script support
    - Enables running of simple shell scripts
    - Checks for execution permissions
    - Handles script arguments


## Code Layout
- `bin/`
    - `pennfat` (executable)
    - `pennos` (executable)
- `doc/`
    - `README.md`
- `log/`
    - *generated log files*
- `src/` 
    - `fs/`
        - `fat_routines.c`
        - `fat_routines.h`
        - `fs_helpers.c`
        - `fs_helpers.h`
        - `fs_kfuncs.c`
        - `fs_kfuncs.h`
        - `fs_syscalls.c`
        - `fs_syscalls.h`
    - `kernel/`
        - `kern_pcb.c`
        - `kern_pcb.h`
        - `kern_sys_calls.c`
        - `kern_sys_calls.h`
        - `logger.c`
        - `logger.h`
        - `scheduler.c`
        - `scheduler.h`
        - `signal.c`
        - `signal.h`
        - `stress.c`
        - `stress.h`
    - `lib/`
        - `pennos-errno.c`
        - `pennos-errno.h`
        - `spthread.c`
        - `spthread.h`
        - `Vec.c`
        - `Vec.h`
    - `shell/`
        - `builtins.c`
        - `builtins.h`
        - `Job.h`
        - `parser.c`
        - `parser.h`
        - `shell_built_ins.c`
        - `shell_built_ins.h`
        - `shell.c`
        - `shell.h`
    - `pennfat.c`
    - `pennos.c`
- `tests/`
    - `sched-demo.c`
- `.gitignore`
- `Makefile`

## Code Description and Design Justifications

### PennFAT Filesystem

- **fat_routines**
    - `mkfs`: 
        - *Inputs*: filename, number of blocks, and size of each block
        - *Output*: 0 on success, -1 on failure
        - *Description*: Makes a filesystem. Uses the inputs to calculate the size of the FAT region, data region, and the filesystem. It then makes a system call to open() to open the filename and use ftruncate() to extend the size of the filesystem. A combination of calloc, lseek, and write are used to allocate space for the fat region and root directory and write the contents to the filesystem. 
    - `mount`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `unmount`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `cat`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `ls`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `touch`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `mv`: 
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `cp`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `rm`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `chmod`:
        - *Inputs*:
        - *Output*:
        - *Description*:
    - `cmpctdir`:
        - *Inputs*:
        - *Output*:
        - *Description*:
- **fs_helpers**
    - todo
- **fs_kfuncs**
    - todo
- **fs_syscalls**
    - todo

### Kernel

### Shell

## General Comments
- N/A