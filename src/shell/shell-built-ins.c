#include "shell-built-ins.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>






////////////////////////////////////////////////////////////////////////////////
//     Zombie and orphan testing functions                                    //
////////////////////////////////////////////////////////////////////////////////



/**
 * @brief Helper for zombify.
 */
void* zombie_child(void* arg) {
    return NULL; // TODO --> this was originally just 'return', update if needed
}
  
void* zombify(void* arg) {
    //s_spawn(zombie_child, ...); TODO --> fix/fill in args
    while (1);
    return NULL; // TODO --> this was originally just 'return', update if needed
}
  
/** 
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
    while (1);
}

void* orphanify(void* arg) {
    // s_spawn(orphan_child, ...); TODO --> fix/fill in args
    return NULL; // TODO --> this was originally just 'return', update if needed
}