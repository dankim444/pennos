#include "src/lib/Vec.h"
#include "src/kernel/kern-pcb.h"
#include "src/kernel/scheduler.h"
#include <stdbool.h>


int main() {

    // initialize scheduler architecture and init process
    initialize_scheduler_queues();
    // TODO --> initialzie init process

    scheduler();

    // cleanup
    free_scheduler_queues();
    // TODO --> add anything else that's needed
}