#ifndef LOGGER_H_
#define LOGGER_H_


/**
 * @brief Logs a scheduling event i.e. the scheduling of a process for 
 *        this clock tick
 * 
 * @param pid          pid of the process being scheduled
 * @param queue_num    the priority queue num of the process 
 * @param process_name string containing scheduled process's name
 * 
 * @post will perror if the write fails
 */
void log_scheduling_event(int pid, int queue_num, char* process_name);

/**
 * @brief Logs a non-nice, non-scheduling event (i.e any event that follows
 *        the EVENT PID NICE_VALUE PROCESS_NAME format)
 * 
 * @param event_type the type of event, defined by:
 *                   'C' = CREATE, 'S' = SIGNALED, 'E' = EXITED,
 *                   'Z' = ZOMBIE, 'O' = ORPHAN, 'W' = WAITED
 *                   'B' = BLOCKED, 'U' = UNBLOCKED
 *                   's' = STOPPED, 'c' = CONTINUED (notably lower-cased)
 * @param pid          process pid
 * @param nice_value   process nice value
 * @param process_name string containing process name
 * 
 * @pre assums event_type matches one of the above characters
 * @post will perrorr if the write fails
 */
void log_generic_event(char event_type, int pid, int nice_value, char* process_name);

/**
 * @brief Logs a nice event, which is the adjusting of a process's nice value
 * 
 * @param pid            process pid
 * @param old_nice_value old nice value 
 * @param new_nice_value new nice value
 * @param process_name   string containing process name
 * 
 * @post will perror if the write fails
 */
void log_nice_event(int pid, int old_nice_value, int new_nice_value, char* process_name);


#endif