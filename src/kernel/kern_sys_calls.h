/* CS5480 PennOS Group 61
 * Authors: Krystof Purtell and Richard Zhang
 * Purpose: Defines the system-level process-related kernel functions. 
 */

#ifndef KERN_SYS_CALLS_H_
#define KERN_SYS_CALLS_H_

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "kern_pcb.h"

////////////////////////////////////////////////////////////////////////////////
//                         GENERAL HELPER FUNCTIONS                           //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Given a thread pid and Vec* queue, this helper function determines
 *        the vector index of the thread/pid in the queue. If the thread/pid
 *        is not found, it returns -1.
 *
 * @param queue pointer to the vector queue that may contain the thread/pid
 * @param pid   the thread's pid
 *
 * @return the index of the thread/pid in the queue, or -1 if not found
 */
int determine_index_in_queue(Vec* queue, int pid);

/**
 * @brief Given a thread's previous priority, this helper checks if the
 *        thread is present in that priority's queue, removes it from that
 *        queue if so, and then puts it into the new priority level's queue.
 *
 * @param prev_priority thread's previous priority
 * @param new_priority  thread's new priority
 * @param curr_pcb     pointer to the thread's PCB
 *
 * @pre assumes the prev_priority and new_priority falls in integers [0, 2]
 */
void move_pcb_correct_queue(int prev_priority, int new_priority, pcb_t* curr_pcb);

/**
 * @brief Deletes the PCB with the specified PID from one of the priority
 * queues, selected by the provided queue_id (0, 1, or 2).
 *
 * @param queue_id An integer representing the queue: 0 for zero_priority_queue,
 *                 1 for one_priority_queue, or 2 for two_priority_queue.
 * @param pid The PID of the PCB to be removed.
 */
void delete_from_queue(int queue_id, int pid);

/**
 * @brief Helper function that deletes the given PCB from the explicit queue
 *        passed in. Notably, it does not free the PCB but instead uses
 *       vec_erase_no_deletor to remove it from the queue.
 * 
 * @param queue_to_delete_from ptr to Vec* queue to delete from
 * @param pid                  the pid of the PCB to delete
 */
void delete_from_explicit_queue(Vec* queue_to_delete_from, int pid);

/**
 * @brief The init process function. It spawns the shell process and 
 *        reaps zombie children.
 * 
 * @param input unused but needed for typing reasons
 * @return irrelvant return value because never supposed to return
 */ 
void* init_func(void* input);


////////////////////////////////////////////////////////////////////////////////
//               SYSTEM-LEVEl PROCESS-RELATED KERNEL FUNCTIONS                //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Similar to s_spawn except only called when you want to spawn the init
 *        process. It will create the init process and also spawn in the 
 *        shell. 
 * 
 * @return the pid_t of the created process on success or -1 on error
 */
pid_t s_spawn_init();


/**
 * @brief Wrapper system-level function to be called in pennos's
 *        main method to clean up the init process
 */
void s_cleanup_init_process();

/**
 * @brief Create a child process that executes the function `func`.
 *        The child will retain some attributes of the parent.
 *
 * @param func  Function to be executed by the child process.
 * @param argv  Null-terminated array of args, including the command name as argv[0].
 * @param fd0   Input file descriptor.
 * @param fd1   Output file descriptor.
 * @return pid_t The process ID of the created child process or -1 on error
 */
pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1);

/**
 * @brief Wait on a child of the calling process, until it changes state.
 *        If `nohang` is true, this will not block the calling process and 
 *        return immediately.
 *
 * @param pid Process ID of the child to wait for.
 * @param wstatus Pointer to an integer variable where the status will be stored.
 * @param nohang If true, return immediately if no child has exited.
 * @return pid_t The process ID of the child which has changed state on success, -1 on error.
 */
pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang);

/**
 * @brief Send a signal to a particular process.
 *
 * @param pid Process ID of the target proces.
 * @param signal Signal number to be sent 
 *               0 = P_SIGSTOP, 1 = P_SIGCONT, 2 = P_SIGTERM
 * @return 0 on success, -1 on error.
 */
int s_kill(pid_t pid, int signal);

/**
 * @brief Unconditionally exit the calling process.
 */
void s_exit(void);

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority);

/**
 * @brief Suspends execution of the calling proces for a specified number of clock ticks.
 *
 * This function is analogous to `sleep(3)` in Linux, with the behavior that the system
 * clock continues to tick even if the call is interrupted.
 * The sleep can be interrupted by a P_SIGTERM signal, after which the function will
 * return prematurely.
 *
 * @param ticks Duration of the sleep in system clock ticks. Must be greater than 0.
 */
void s_sleep(unsigned int ticks);


////////////////////////////////////////////////////////////////////////////////
//              SYSTEM-LEVEl BUILTIN-RELATED KERNEL FUNCTIONS                 //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief System-level wrapper for the shell built-in command "echo" 
 * 
 * @param arg the pass along arguments to the u_echo function
 * @return NULL, dummy return value
 */
void* s_echo(void* arg);

/**
 * @brief System-level wrapper for the shell built-in command "ps" 
 * 
 * @param arg the pass along arguments to the u_ps function
 * @return NULL, dummy return value
 */
void* s_ps(void* arg);



#endif 