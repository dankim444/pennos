#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include "parser.h"

/**
 * Ignores SIGINT and SIGTSTP signals during fork.
 */
void ignore_signals_during_fork(void);

/**
 * Restores default signal handlers after fork.
 */
void restore_signals_after_fork(void);

/**
 * Creates the specified number of pipes.
 *
 * @param pipeCount Number of pipes to create (usually num_commands - 1).
 * @param pipeFds Pointer to an integer array storing pipe file descriptors.
 */
void create_pipes(size_t pipeCount, int* pipeFds);

/**
 * Spawns child processes for each command in the pipeline.
 *
 * @param parsedResult Pointer to parsed command metadata.
 * @param commands_count Number of commands in the pipeline.
 * @param pipeFds Array of file descriptors for pipes.
 * @param pids Array to store the PIDs of spawned children.
 * @param pgid Pointer to store the pipeline's process group ID.
 */
void spawn_pipeline_children(struct parsed_command* parsedResult,
                             size_t commands_count,
                             int* pipeFds,
                             pid_t* pids,
                             pid_t* pgid);

/**
 * Closes all pipe file descriptors in the parent process.
 *
 * @param pipeCount Number of pipes (num_commands - 1).
 * @param pipeFds Array of pipe file descriptors to close.
 */
void close_parent_pipes(size_t pipeCount, int* pipeFds);

/**
 * Handles foreground execution in an interactive shell, giving the job
 * control of the terminal and waiting for it to finish.
 *
 * @param parsedResult Pointer to parsed command metadata.
 * @param pgid Process group ID of the pipeline.
 * @param pids Array of child PIDs.
 * @param commands_count Number of commands in the pipeline.
 */
void handle_interactive_fg(struct parsed_command* parsedResult,
                           pid_t pgid,
                           pid_t* pids,
                           size_t commands_count);

/**
 * Handles background execution in an interactive shell, printing status
 * and storing job information without blocking.
 *
 * @param parsedResult Pointer to parsed command metadata.
 * @param pgid Process group ID of the pipeline.
 * @param pids Array of child PIDs (read-only).
 * @param commands_count Number of commands in the pipeline.
 */
void handle_interactive_bg(struct parsed_command* parsedResult,
                           pid_t pgid,
                           const pid_t* pids,
                           size_t commands_count);

/**
 * Executes a parsed command, handling pipelines, redirection, etc.
 *
 * @param parsed_result Pointer to a struct containing metadata about the parsed
 * command.
 * @param interactive_mode Indicates whether the shell is in interactive mode.
 */
void execute(struct parsed_command* parsed_result, bool interactive_mode);

#endif
