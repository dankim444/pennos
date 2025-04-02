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
- to-do

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
- A programming API for process-related and scheduler functions
- PCB creation and cleanup
- Process statuses and signals (stop, continue, terminate)
- Priority-based scheduler
- SIGALARM-based clock ticks
- Sleep and wait functionality

### File System
- File parsing and mounting
- Root directory only
- File creation, read, write, seek, unlink
- Global file descriptor table
- Kernel-level functions and user-level API for creating and manipulating files
- Command routines (cd, mv, etc.)

### Shell
- Built-ins: cat, echo, sleep, mv, cp, rm, chmod, ls
- Job control: bg, fg, jobs
- Shell script execution
- I/O redirection and basic signal handling

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