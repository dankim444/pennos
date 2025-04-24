#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdbool.h>
#include "../lib/Vec.h"
#include "../lib/spthread.h"
#include "kern_pcb.h"

/**
 * @brief Initializes the scheduler queues. This function should be called
 *        before any other scheduler functions are called.
 */
void initialize_scheduler_queues();

/**
 * @brief Frees the scheduler queues. This function should be called when
 *        the scheduler is no longer needed.
 */
void free_scheduler_queues();

/**
 * @brief Deterministically chooses an integer from 0, 1, 2 at the prescribed
 *        probabilites. In particular, 0 is output 1.5x more than
 *        1, which is output 1.5x more than 2. Notably, it accounts
 *        for cases where some of the queues are empty. If all queues are empty,
 *        it'll return -1.
 *
 * @pre assumes that at least one of the scheduler queues in non-empty
 * @return int 0, 1, or 2 for priority or -1 to signify that all queues 
 *         are empty
 */
int generate_next_priority();

/**
 * @brief Returns the next PCB in the queue of the specified priority. 
 *        or NULL if that queue is empty. Notably, it removes the PCB
 *        from the queue.
 * 
 * @param priority queue priority to get next PCB from, or -1 if none
 * @return         a ptr to the next pcb struct in queue or NULL
 *                 if the queue is empty
 */
pcb_t* get_next_pcb(int priority);

/**
 * @brief Puts the given pcb struct pointer into its appropriate
 *        queue. Notably, it solely uses the pcb's interal fields
 *        to determine the correct queue (priority and state).
 */
void put_pcb_into_correct_queue(pcb_t* pcb);

/**
 * @brief Given a queue in the form of a vector, searches through
 *        it for the given pcb and deletes it from the queue if
 *        found. Notably, it does not free the pcb. Instead, the
 *        implmentation calls vec_erase_no_deletor instead of
 *        vec_erase. If the pcb isn't in the queue, this function
 *        does nothing.
 */
void delete_process_from_particular_queue(pcb_t* pcb, Vec* queue);

/**
 * @brief Searches through all of the scheduler's queues except the one 
 *        containing all of the current processes and deletes the given 
 *        pcb from all of them Notably, it does not free the pcb via 
 *        calling vec_erase_no_deletor instead of vec_erase. If
 *        a particular queue does not contain the pcb, nothing occurs.
 *
 * @param pcb a pointer to the pcb with the pid to delete
 */
void delete_process_from_all_queues_except_current(pcb_t* pcb);

/**
 * @brief Searches through all of the scheduler's queues and deletes the
 *        the given pcb from all of them. Notably, it does not free the
 *        pcb via calling vec_erase_no_deletor instead of vec_erase. If
 *        a particular queue does not contain the pcb, nothing occurs.
 *
 * @param pcb a pointer to the pcb with the pid to delete
 */
void delete_process_from_all_queues(pcb_t* pcb);


/**
 * @brief Given a queue, searches for a particular pid inside that queue
 *        and, if found, returns the pcb_t* associated with that pid.
 *
 * @param queue the queue of pcb_t* ptrs to search
 * @param pid   the pid to search for
 * @return a ptr to the pcb w/ the desired pid if found, NULL otherwise
 */
pcb_t* get_pcb_in_queue(Vec* queue, pid_t pid);

/**
 * @brief TODO
 */
void scheduler();

/**
 * @brief TODO
 */
void shutdown_pennos();

// DELETE on submission
void print_all_queues();

#endif  // SCHEDULER_H_
