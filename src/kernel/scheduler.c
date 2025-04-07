#include "scheduler.h"
#include <stdbool.h>
#include "../lib/spthread.h"
#include "../lib/Vec.h"
#include "kern_pcb.h"
#include "stdlib.h"
#include <sys/time.h>
#include <signal.h> // TODO --> make sure this is ok to include



/////////////////////////////////////////////////////////////////////////////////
//                       QUEUES AND SCHEDULER DATA                             //
/////////////////////////////////////////////////////////////////////////////////



Vec zero_priority_queue; // lower index = more recent 
Vec one_priority_queue;
Vec two_priority_queue;
Vec zombie_queue;
Vec sleep_blocked_queue;

Vec current_pcbs; // holds all currently running processes, for logging

static const int hundred_millisec = 100000; // 100 milliseconds
static bool scheduling_done = false; // true if the scheduler is done

int tick_counter = 0;
int log_fd; // file descriptor for the log file, set in pennos.c


/////////////////////////////////////////////////////////////////////////////////
//                         QUEUE MAINTENANCE FUNCTIONS                         //
/////////////////////////////////////////////////////////////////////////////////



void initialize_scheduler_queues() {
    zero_priority_queue = vec_new(0, free_pcb);
    one_priority_queue = vec_new(0, free_pcb);
    two_priority_queue = vec_new(0, free_pcb);
    zombie_queue = vec_new(0, free_pcb);
    sleep_blocked_queue = vec_new(0, free_pcb);
    current_pcbs = vec_new(0, free_pcb);
}

void free_scheduler_queues() {
    vec_destroy(&zero_priority_queue);
    vec_destroy(&one_priority_queue);
    vec_destroy(&two_priority_queue);
    vec_destroy(&zombie_queue);
    vec_destroy(&sleep_blocked_queue);
    vec_destroy(&current_pcbs);
}



/////////////////////////////////////////////////////////////////////////////////
//                         SCHEDULING FUNCTIONS                                //
/////////////////////////////////////////////////////////////////////////////////


int generate_next_priority() {

    // only 1 non-empty cases
    if (vec_is_empty(&zero_priority_queue) && vec_is_empty(&one_priority_queue)) {
        return 2;
    } else if (vec_is_empty(&zero_priority_queue) && vec_is_empty(&two_priority_queue)) {
        return 1;
    } else if (vec_is_empty(&one_priority_queue) && vec_is_empty(&two_priority_queue)) {
        return 0;
    }

    // all 3 non-empty case
    if (!vec_is_empty(&zero_priority_queue) && !vec_is_empty(&one_priority_queue) && !vec_is_empty(&two_priority_queue)) {

        double rand_num = (double) rand() / (double)RAND_MAX * 4.75; // rand num in [0, 4.75]

        if (rand_num <= 2.25) {
            return 0;
        } else if (rand_num <= 3.75) {
            return 1;
        } else {
            return 2;
        }
    }

    // 2 non-empty cases
    double rand_proportion = (double) rand() / (double)RAND_MAX; // rand num in [0, 1]
    if (vec_is_empty(&zero_priority_queue) && !vec_is_empty(&one_priority_queue)) {
        if (rand_proportion <= .60) {
            return 0;
        } else {
            return 1;
        }
    } else if (vec_is_empty(&one_priority_queue) && !vec_is_empty(&two_priority_queue)) {
        if (rand_proportion <= .60) {
            return 1;
        } else {
            return 2;
        }
    } else if (vec_is_empty(&zero_priority_queue) && !vec_is_empty(&two_priority_queue)) {
        if (rand_proportion <= .6923) {
            return 0;
        } else {
            return 2;
        }
    }

    return -1; // should never reach here
}

pcb_t* get_next_pcb(int priority) {
    pcb_t* next_pcb = NULL;

    if (priority == 0) {
        next_pcb = vec_get(&zero_priority_queue, 0);
        vec_erase_no_deletor(&zero_priority_queue, 0);
    } else if (priority == 1) {
        next_pcb = vec_get(&one_priority_queue, 0);
        vec_erase_no_deletor(&one_priority_queue, 0);
    } else if (priority == 2) {
        next_pcb = vec_get(&two_priority_queue, 0);
        vec_erase_no_deletor(&two_priority_queue, 0);
    }

    return next_pcb;
}


void put_pcb_into_correct_queue(pcb_t* pcb) {
    if (pcb->process_state == 'R') {
        if (pcb->priority == 0) {
            vec_push_back(&zero_priority_queue, pcb);
        } else if (pcb->priority == 1) {
            vec_push_back(&one_priority_queue, pcb);
        } else if (pcb->priority == 2) {
            vec_push_back(&two_priority_queue, pcb);
        }
    } else if (pcb->process_state == 'Z') {
        vec_push_back(&zombie_queue, pcb);
    } else if (pcb->process_state == 'B' || pcb->process_state == 'S') {
        vec_push_back(&sleep_blocked_queue, pcb);
    }
}

void delete_process_from_particular_queue(pcb_t* pcb, Vec* queue) {
    for (int i = 0; i < vec_len(queue); i++) {
        pcb_t* curr_pcb = vec_get(queue, i);
        if (curr_pcb->pid == pcb->pid) {
            vec_erase_no_deletor(queue, i);
            return;
        }
    }
}

void delete_process_from_all_queues(pcb_t* pcb) {
    delete_process_from_particular_queue(pcb, &zero_priority_queue);
    delete_process_from_particular_queue(pcb, &one_priority_queue);
    delete_process_from_particular_queue(pcb, &two_priority_queue);
    delete_process_from_particular_queue(pcb, &zombie_queue);
    delete_process_from_particular_queue(pcb, &sleep_blocked_queue);
    delete_process_from_particular_queue(pcb, &current_pcbs);
}

pcb_t* get_pcb_in_queue(Vec* queue, pid_t pid) {
    for (int i = 0; i < vec_len(queue); i++) {
        pcb_t* curr_pcb = vec_get(queue, i);
        if (curr_pcb->pid == pid) {
            return curr_pcb;
        }
    }

    return NULL;
}

void alarm_handler(int signum) {
    tick_counter++;
}

void scheduler() {

    // TODO --> insert code somewhere to prevent busy waiting via sigsuspend
    // Is init always running? Need to figure this out

    int curr_priority_queue_num;

    // mask for while scheduler is waiting for alarm
    sigset_t suspend_set;
    sigfillset(&suspend_set);
    sigdelset(&suspend_set, SIGALRM);

    // ensure sigarlm doesn't terminate the process
    struct sigaction act = (struct sigaction){
        .sa_handler = alarm_handler,
        .sa_mask = suspend_set,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGALRM, &act, NULL);

    // make sure SIGALRM is unblocked
    sigset_t alarm_set;
    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = hundred_millisec * 10};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);

    while(!scheduling_done) {
        curr_priority_queue_num = generate_next_priority();
        pcb_t* curr_pcb = get_next_pcb(curr_priority_queue_num);

        spthread_continue(curr_pcb->thread_handle);
        sigsuspend(&suspend_set);
        spthread_suspend(curr_pcb->thread_handle);

        // TODO --> probably will need some way to track state here +
        // determine which queue to put the pcb into
        put_pcb_into_correct_queue(curr_pcb);
    }
}











