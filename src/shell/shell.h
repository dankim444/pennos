#ifndef SHELL_H_
#define SHELL_H_

/**
 * @brief The function that runs our shell. TODO rest
 *
 */
void* shell(void* input);

/* Polls background jobs to see if any have completed.
 *
 */
void poll_background_jobs(void);

#endif