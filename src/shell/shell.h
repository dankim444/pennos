/* CS5480 PennOS Group 61
 * Authors: Krystof Purtell and Richard Zhang
 * Purpose: Implements a shell that can run built-in commands and scripts.
 */


#ifndef SHELL_H_
#define SHELL_H_

/**
 * @brief The main shell function that runs the shell. This is run via 
 *        an s_spawn call from init's process. It prompts the user for 
 *        builtins and scripts to run.
 * 
 * @param input unused
 */
void* shell(void* input);

#endif