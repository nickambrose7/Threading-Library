#include <schedulers.h> // contains some scheduler variables (may not need)
#include <rr.h> // contains the round robin variable/struct defintion
#include <lwp.h> // contains the scheduler function definitions
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// to implement the round robin and scheduler functions

// head is the currently running thread
// making linkedlist with what the head and length of the list
typedef struct schedule {
    thread head;
    int qlen;
} schedule;

schedule *our_schedule;


void init(void) {
    /* initialize any structures     */
    our_schedule = (schedule *)malloc(sizeof(schedule));
    our_schedule->qlen = 0;
}

void shutdown(void){        
    /* tear down any structures      */
    free(our_schedule);
}

void admit(thread new){
    /* add a thread to the pool      */

    // check if we have empty thread pool
    if (our_schedule->qlen == 0) {
        our_schedule->head = new;
    }
    else {
        // loop on qlen
        thread curr_thread = our_schedule->head;
        for (int i=0; i<our_schedule->qlen; i++) {
            curr_thread = curr_thread->sched_one;
        }
        // set next to new thread
        curr_thread->sched_one = new;
    }
    // update qlen
    our_schedule->qlen++;

}
void remove(thread victim) {
    /* remove a thread from the pool */

    // check if we have empty thread pool
    if (our_schedule->qlen > 0) {
        // loop on qlen for tid
        thread prev_thread = our_schedule->head;
        thread curr_thread = our_schedule->head;
        int pos = 0;
        while(curr_thread->tid != victim->tid && pos < our_schedule->qlen) {
            prev_thread = curr_thread;
            curr_thread = curr_thread->sched_one;
            pos++;
        }

        if (pos < our_schedule->qlen) {
            // get the next thread to link prev to next, if it exists
            thread next_thread = curr_thread->sched_one;
            // check the next thread
            if (*(next_thread->stack) != NULL) {
                // set next to new thread
                prev_thread->sched_one = next_thread;
            }
            // update qlen
            our_schedule->qlen--;
        }
        else {
            perror("Error finding thread to remove");
            exit(EXIT_FAILURE);
        }
    }
}

thread next(void) {            
    /* select a thread to schedule   */
    return our_schedule->head->sched_one;
}

int qlen(void) {
    /* number of ready threads       */
    // is the readiness of a thread stored in status? loops 
    // through queue and check ready status or return qlen?
    int ready = 0;
    if (our_schedule->qlen == 0) {
        return ready;
    }
    thread curr_thread = our_schedule->head;
    for (int i=0; i<our_schedule->qlen; i++) {
        if (curr_thread->status == LWP_LIVE) {
            ready++;
        }
        curr_thread = curr_thread->sched_one;
    }
    return ready;

}

void roundRobin(void) {

}