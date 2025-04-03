#ifndef JOB_CONTROLLER_H
#define JOB_CONTROLLER_H

#include "Job.h"
#include "parser.h"

/*
 * Element destructor for the job list vector.
 *
 * @param ptr A pointer to the job struct.
 */
void free_job_ptr(void* ptr);

/*
 * Initializes the job list vector.
 *
 */
void init_job_vector(void);

/*
 * Helper function for printing a command.
 *
 * @param status A string representing the status of the job.
 * @param cmd A pointer to the parsed_command struct.
 */
void print_job_status_message(const char* status,
                              const struct parsed_command* cmd);

/*
 * Finds a job by its id or the current job.
 *
 * @param arg A string representing the job id.
 */
job* findJobByIdOrCurrent(const char* arg);

/*
 * Prints the list of jobs.
 *
 */
void builtinJobs(void);

/*
 * Resumes a stopped job in the background.
 *
 * @param jobArg A string representing the job id.
 */
void builtinBg(const char* jobArg);

/*
 * Resumes a stopped job in the foreground.
 *
 * @param jobArg A string representing the job id.
 */
void builtinFg(const char* jobArg);

/*
 * Checks whether the parsed command is a built-in (jobs, bg, fg).
 * If so, handles it and returns true. Otherwise, returns false.
 *
 * @param parsed A pointer to the parsed_command struct
 * @return true if this is a builtin, else false
 */
bool handle_builtin(struct parsed_command* parsed);

#endif
