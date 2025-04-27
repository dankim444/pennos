# PennOS
PennOS is a UNIX-like operating system simulator built to run as a single process on a host OS. Its main features include a priority-based scheduler using the spthread module, a custom FAT-like file system called PennFAT, and a shell with support for job control, system calls, and user-level programs. Though it doesn't boot on hardware, our PennOS simulates real OS abstractions like kernel vs. user land, process/thread management, and file handling.

## Members (Name and Pennkey)
- Dan Kim (pennkey: dankim1)
- Kevin Zhou (pennkey: kzhou1)
- Krystof Purtell (pennkey: krystofp)
- Richard Zhang (pennkey: richz)

## List of Submitted Files
- To-do

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
./bin/pennos myfs.pfat [logfile]
```

## Overview of Work Accomplished

### Kernel
- Includes a programming API for process-related and scheduler functions
- Implements a PCB structure for tracking information about processes
- Maintains process statuses and signals (stop, continue, terminate)
- Implements a priority-based scheduler
- Utilizes SIGALARM-based clock ticks
- Integrates sleep and wait functionality

### File System
- Builds a file descriptor table and system-wide file descriptor table
- Implements kernel-level functions for creating and manipulating files (k_open, k_close, k_read, k_write, k_lseek, k_unlink, k_ls)
- Supports standalone PennFAT commands and routines (mounting, unmounting, touch, cd, mv, rm, cat, cp, chmod, ls, etc.)
- Mounts PennFAT into memory using mmap(2)
- Ensures robust error handling and logging (ie. FS_NOT_MOUNTED, FILE_NOT_FOUND, etc.)
- Integrates file system into user-shell to enable user interaction with user-level system calls

### Shell
- Interacts with OS using user-level functions
- Synchronous Child Waiting: shell waits on all children before re-prompting
- I/O redirection (>, <, and >>)
- Parsing (uses parser implemented in penn-shell)
- Terminal signaling (relays signals like ctrl-z and ctrl-c to s_kill)
- Enforcing terminal control: Tracks the terminal-controlling process using a global variable; background processes reading from stdin are stopped
- Built-ins: cat, echo, sleep, busy, touch, mv, cp, rm, chmod, ps, kill, ls

## Code Layout and Description of Code

### Code Layout
- `Makefile`
- `bin/`
    - `pennfat`
    - `pennos`
    - `sched-demo`
    - *other compiled executables*
- `doc/`
    - `README.md`
    - `CompanionDoc.pdf`
- `log/`
    - *log files*
- `src/`
    - `utils/`
        - `spthread.c`
        - `spthread.h`
    - `pennfat.c`
    - `pennos.c`
- `tests/`
    - `sched-demo.c`
    - *other test files we may create*

### Description of Code and Design
- PCB Structure: See doc/CompanionDocument.pdf for full details
- Data structures: `penn-vec` for queue implementation
- Scheduler: Uses round-robin scheduling with priority multipliers
- FAT: Memory-mapped FAT with support for FAT block chaining
- Abstractions:
    - k_: Kernel-level file system
        - TO-DO: list kernel-level functions
    - s_: System calls (userland interface)
        - TO-DO: list system-level functions
    - u_: Shell-level utilities
        - TO-DO: list user-level functions
- Logging: All system events are recorded in the log/ folder per tick

## General Comments
- to-do

## Attributions
- sched-demo.c code used in scheduler.c