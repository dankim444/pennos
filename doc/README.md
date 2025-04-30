# PennOS
PennOS is a UNIX-like operating system simulator built to run as a single process on a host OS. Its main features include a priority-based scheduler that handles threads using the spthread module, a custom FAT-like file system, and a shell with support for job control, system calls, and user-level programs. Though it doesn't boot on actual hardware, our PennOS simulates real OS abstractions like kernel vs. user land, process/thread management, and file handling.

Document design justifications.

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
- Free memory leaks (only for pennfat)

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
    - *File descriptor entry structure*: Holds metadata about each file descriptor in the system-wide file descriptor table. Tracks open file state including position (indicates where subsequent reads or writes should take place), access mode, and reference counts.
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
    - Note: the only time we use regular system calls (ie. `read`, `lseek`, `write`, etc.) is when we interact with the host OS. For example, in `cp SOURCE -h DEST` we use `k_open()` to open `SOURCE` but `open()` to open `DEST`. However, in `cat` we only use the kernel-level functions we implemented. We use `lseek` and `write` to write to a file in the host OS.
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
    - Implements sigsuspend
- **Logging**
    - Implements event logging for debugging and verification with timestamps

### Shell
The PennOS shell provides a user interface to interact with the simulated operating system, offering a set of built-in commands and job control features.
- **CLI**
    - Prompts user for command and parses arguments
    - Supports redirection
    - Supports fg/bg jobs
    - Handles signals for user interrupts
- **Built-in commands**
    - For files: cat, ls, touch, mv, cp, rm, chmod
    - For processes: ps, kill, nice, nice_pid
    - For jobs: bg, fg, jobs
    - For utilities: sleep, busy, echo, man
    - For testing utils: zombify, orphanify
    - System control: logout
- **Process hierarchy**
    - Shell as parent of user commands
    - Child process spawning and monitoring
    - Zombie reaping and cleanup
- **Script support**
    - Enables running of simple shell scripts
    - Checks for execution permissions
    - Handles script arguments

### Error Handling
An error module is defined in `lib/pennos-errno.h` and `lib/pennos-errno.c`. This module simply defines the set of error codes used throughout our code, and `P_ERRNO` is the variable that tracks the current error code set at any point in the program. `shell/builtins.h` and `shell/builtins.c` define and implement the `u_perror` function, which is used to display user-level error messages to stdout.


## Code Layout
- `bin/`
    - `pennfat` (executable)
    - `pennos` (executable)
- `doc/`
    - `README.md`
    - `companion_doc.pdf`
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

### Overview of Architecture and Core Data Structures
todo

### PennFAT Filesystem

- **fat_routines**
    - `mkfs`: 
        - *Inputs*: filename, number of blocks, and size of each block
        - *Output*: 0 on success, -1 on failure
        - *Description*: Makes a filesystem. Uses the inputs to calculate the size of the FAT region, data region, and the filesystem. Then makes a system call with `open()` to open the filename and uses `ftruncate()` to extend the filesystem's size. A combination of `calloc`, `lseek`, and `write` are used to allocate space for both the fat region and root directory and write their contents to the filesystem. 
    - `mount`:
        - *Input*: filename `fs_name`
        - *Output*: 0 on success, -1 on error
        - *Description*: Mounts the specified filesystem. Opens the filesystem and stores the file descriptor returned by `open()` in `fs_fd`. Most notably, `mount` will call `mmap()` to map the FAT region into memory, initialize the system-wide file descriptor table, and initialize the other global variables `block_size`, `num_fat_blocks`, `fat_size`, `fat`, `is_mounted`, and `MAX_FDS`.
    - `unmount`:
        - *Input*: N/A
        - *Output*: 0 on success, -1 on error
        - *Description*: Unmounts the currently mounted filesystem. Uses `munmap` to unmap the fat region, closes `fs_fd`, and resets the globals.
    - `cat`:
        - *Input*: Void pointer to a list of arguments
        - *Output*: Void pointer
        - *Description*: Concatenates and displays files. First, scans the list of arguments and opens the output file with `k_open`. If the output flag is -w, then we overwrite the output file; if the output flag is -a, then we append to the output file. If no output redirection is supplied, then we write to stdout. If there is at least one input file, we process and write each input file to the output file using `k_open`; otherwise, we simply read from stdin and write to the output file.
    - `ls`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Lists all files in the root directory. Keeps track of a `current_block` that starts at 1, denoting the root directory. Starting at the first block, use read every directory entry and print its content to stdout. We keep updating `current_block` to the next block and repeat the process until the fat chain ends. 
    - `touch`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Creates files or updates timestamps. For each file supplied as an argument, we first determine if the file already exists in the root directory. If the file exists, update its timestamp. Otherwise, add its file entry to the root directory. 
    - `mv`: 
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Renames a source file to a destination file. If the source and destination files have the same name, then return. If the source file is not in the root directory, then we return from the routine and set the error code. Next, we check if the destination file exists. If it exists, then we mark the destination file entry as deleted, only if it not currently in use by any other fd. Lastly, rename the source entry's filename and write the change to the root directory of the filesystem.
    - `cp`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Copies a source file to a destination. If the command is of the form `cp -h SOURCE DEST`, then we open `SOURCE` using `open()` and `DEST` with `k_open()`. While the bytes remaining in `SOURCE` is greater than zero, we use `read()` to read a certain number of bytes from `SOURCE` and use `k_write()` to write the same number of bytes to `DEST`. The buffer size we choose does not exceed the `block_size` of the currently mounted filesystem. The logic follows similarly for `cp SOURCE -h DEST` and `cp SOURCE DEST`. 
    - `rm`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Removes files. For each file supplied as an argument, we mark the file entry as deleted and write the change to the root directory of the filesystem. Before we can mark a file entry as deleted, we must first check that it exists using the `find_file()` helper function, which returns the absolute offset of where the file entry is located in the filesystem. We use this absolute offset as a parameter in `lseek()` and then use `write()` to commit the change.
    - `chmod`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Changes file permissions. Parses the permission string (e.g., +rw, -x) to determine which permissions to add or remove. Locates the file's directory entry, updates the permission byte according to the requested changes, and writes the updated entry back to disk. Handles error cases such as non-existent files or invalid permission specifications appropriately.
    - `cmpctdir`:
        - *Inputs*: Void pointer to a list of arguments.
        - *Output*: Void pointer
        - *Description*: Compacts the directory structure by removing gaps left by deleted files. Traverses the root directory blocks, identifies deleted entries (marked with 1 or 2 in the first byte), and rearranges valid entries to eliminate gaps. Ensures all directory entries remain in a contiguous sequence, which improves directory traversal performance. This extra credit feature optimizes filesystem storage by reducing fragmentation in the directory structure.
- **fs_helpers**
    - `init_fd_table`: 
        - *Inputs*: A pointer to the system-wide fd table
        - *Output*: N/A
        - *Description*: Initializes all entries in the file descriptor table. Note that the first three file descriptors are reserved for stdin, stdout, and stderr, respectively. The other file descriptor entries' fields will be flushed and set to not in use.
    - `get_free_fd`:
        - *Inputs*: A pointer to the system-wide fd table
        - *Output*: The index to the first free fd; -1 if none available.
        - *Description*: Used in `k_open()`. Iterates through the file descriptor array until we find the first un-used fd and return its index. 
    - `increment_fd_ref_count`:
        - *Input*: The fd number
        - *Output*: The new reference count or -1 on error.
        - *Description*: Used in shell. Increments the reference count of a file descriptor that is in use and returns the fd's reference count.
    - `decrement_fd_ref_count`:
        - *Inputs*: The fd number
        - *Output*: The new reference count or -1 on error.
        - *Description*: Used in shell. Decrements the reference count of a file descriptor that is in use and returns the fd's reference count.
    - `has_executable_permission`:
        - *Inputs*: The fd number
        - *Output*: 1 if the file has executable permissions, 0 if it doesn't, -1 if an error occurred.
        - *Description*: Checks if a file has executable permissions. Checks if the file entry exists and returns 1 if the file has executable permissions; 0 otherwise.
    - `allocate_block`:
        - *Inputs*: N/A
        - *Output*: The block number of the allocated block; 0 if there are no free blocks available.
        - *Description*: Returns the first free block in the FAT table and marks it as used. If there are no free blocks, we try compacting the directory and try again. Otherwise, return 0.
    - `find_file`:
        - *Inputs*: The filename to search for; the output parameter for the file entry
        - *Output*: Absolute offset of the file in the filesystem.
        - *Description*: Searches for the file entry with the filename supplied as an argument and points the output parameter to this file entry if found. Follows the FAT chain for the root directory. For each block, we process each file entry in the current block and check the current file entry's filename. If we reach the end of a block, we move to the next block as permitted by the FAT chain, then continue the process until we find the desired file.
    -  `add_file_entry`
        - *Inputs*: filename, size, first block, type, and permissions
        - *Output*: Offset of the file entry that was added in the filesystem
        - *Description*: Adds a new file entry to the root directory. First checks if the file already exists by calling `find_file()`. Then searches through the root directory blocks for a free slot (either an empty entry or a deleted entry marked with 1). If a free slot is found, it initializes a new directory entry with the provided parameters, sets the modification time to the current time, and writes the entry to disk. If no free slot is found in existing blocks, it allocates a new block, links it to the directory chain, and adds the entry there.
    - `mark_entry_as_deleted`
        - *Inputs*: A pointer to the file entry; the absolute offset of the file entry's position
        - *Output*: 0 on success, -1 on error
        - *Description*: Marks a file entry entry as deleted, writes the changes to the root directory, and frees the relevant blocks. Follows the FAT chain, starting at the file entry's first block, and sets each block as free. Mark the file entry as deleted by setting the first slot of the filename as 0. Move the filesystem's pointer to the absolute offset using `lseek()` and use `write()` to write the changes to the filesystem.
    - `copy_host_to_pennfat`
        - *Inputs*: A pointer to the host filename, a pointer to the pennfat filename
        - *Output*: 0 on success, -1 on error
        - *Description*: Used in the `cp` routine. Copies data from host OS file to the PennFAT file. Uses `open()` to open the host filename for reading and `k_open()` to open the pennfat filename. While the bytes remaining in the host filename is greater than 0, we use `read()` to read a certain number of bytes from the host filename and use `k_write()` to write the same number of bytes to the pennfat filename. Properly handles errors and ensures resource cleanup.
    - `copy_pennfat_to_host`
        - *Inputs*: Source filename in the PennFAT filesystem and the destination path on the host OS
        - *Output*: 0 on success, -1 on error
        - *Description*: Copies a file from the PennFAT filesystem to the host OS. Opens the source file in PennFAT using `k_open()` and creates the destination file on the host filesystem using standard `open()` with appropriate flags for creation and truncation. Allocates a buffer for data transfer and copies the file content in chunks, using `k_read()` to read from PennFAT and standard `write()` to write to the host file. Properly handles errors and ensures resource cleanup.
    - `copy_source_to_dest`
        - *Inputs*: Source and destination filenames, both within the PennFAT filesystem
        - *Output*: 0 on success, -1 on error
        - *Description*: Copies a file within the PennFAT filesystem. Opens the source file for reading and the destination file for writing using `k_open()`. Transfers data in chunks using a buffer, reading from the source with `k_read()` and writing to the destination with `k_write()`. This function is simpler than the cross-filesystem copies as it doesn't need to manage different file system interfaces. Ensures proper error handling and resource cleanup throughout the operation.
- **fs_kfuncs**
    - `k_open`: 
        - *Inputs*: A pointer to the filename, and the read mode (F_READ, F_WRITE, and F_APPEND)
        - *Output*: A fd on success, -1 on error. 
        - *Description*: Opens a filename and returns the associated fd. First ensures that there is a free, un-used fd from the fd table using `get_free_fd()`. If the file doesn't exist and the mode is not F_WRITE, then we set the error code and return -1. If the file doesn't exist but the mode is F_WRITE, we allocate the first available block using `allocate_block()`, add the file entry to the root directory using `add_file_entry()`, and initializes the fd entry in the fd table.
    - `k_read`:
        - *Inputs*: The file descriptor of the open file, the buffer to store the read data, and the number of bytes
        - *Output*: The number of bytes read on success, -1 on error
        - *Description*: Reads data from an open file. Validates the file descriptor and buffer, then determines how many bytes can actually be read based on the current file position and size. Navigates to the correct block in the file's chain by following the FAT entries, and positions at the appropriate offset within that block. Reads data in chunks, potentially spanning multiple blocks if necessary. Updates the file position after reading and handles edge cases like EOF and block boundaries. Returns the total number of bytes read or appropriate error codes. 
    - `k_write`:
        - *Inputs*: The file descriptor, a pointer to the data buffer, and the number of bytes to write
        - *Output*: The number of bytes written on success, -1 on error
        - *Description*: Writes data to an open file. Validates the file descriptor and input buffer, then prepares for writing by calculating the current block and offset. If the file doesn't have a first block yet, it allocates one. Navigates to the appropriate position in the file's block chain, allocating new blocks as necessary when crossing block boundaries. Uses a read-modify-write approach for partial block writes to preserve existing data. Updates the file size if the write extends beyond the current end of file, and updates the directory entry accordingly. Returns the number of bytes successfully written.
    - `k_close`:
        - *Inputs*: The file descriptor to close
        - *Output*: 0 on success, -1 on error
        - *Description*: Closes an open file and releases its file descriptor. Validates that the file descriptor is in use, then ensures any pending changes are written to disk by updating the directory entry with the current file size and modification time. Marks the file descriptor as not in use, making it available for reuse by future `k_open` calls. Returns 0 on successful closure or an appropriate error code if the file descriptor is invalid.
    - `k_unlink`:
        - *Inputs*: The name of the file to remove
        - *Output*: 0 on success, -1 on error
        - *Description*: Removes a file from the filesystem. Verifies the filename is valid and the filesystem is mounted. Checks if the file is currently open by examining the file descriptor table, and returns an error if any process is using the file. Locates the file's directory entry, marks it as deleted by setting its first byte to 1, and frees all blocks in the file's chain by traversing the FAT and setting each block to `FAT_FREE`. Returns 0 on successful deletion or an appropriate error code.
    - `k_lseek`:
        - *Inputs*: The file descriptor, the offset value, and the reference position (SEEK_SET, SEEK_CUR, or SEEK_END)
        - *Output*: The new file position on success, -1 on error
        - *Description*: Repositions the file offset within an open file. Validates the file descriptor and calculates the new position based on the whence parameter: `SEEK_SET` positions relative to the beginning of the file, `SEEK_CUR` relative to the current position, and `SEEK_END` relative to the end of the file. Verifies the resulting position is valid (not negative), updates the file descriptor's position field, and returns the new position. Returns an appropriate error code if the file descriptor is invalid or the resulting position would be negative.
    - `k_ls`:
        - *Inputs*: The name of a file to list, or NULL to list all files in the current directory
        - *Output*: 0 on success, -1 on error
        - *Description*: Lists files or file information in the current directory. First checks if the filesystem is mounted. If a specific filename is provided, it locates that file's directory entry using `find_file()` and displays its detailed information. If NULL is provided, it traverses the entire root directory structure, following the FAT chain if necessary, and displays information about each valid file entry (skipping deleted entries). For each file, it formats information including block number, permissions, size, timestamp, and name, then writes this information to standard output using `k_write()`. Returns 0 on success or an appropriate error code.
- **fs_syscalls**
    - These functions are simply wrappers around the kernel functions.

### Kernel

### Shell

## General Comments
- extra credit