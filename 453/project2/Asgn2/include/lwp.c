#include "lwp.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/resource.h>
#define _GNU_SOURCE
#include <sys/mman.h>

// define global variables

// global scheduler
scheduler schedule = NULL;

// global threadpool --> make thread pointer, maybe static
thread head = NULL; // head of the thread pool, using the sched_one pointer
// the thread we are currently executing should be at the back of the list

thread terminated = NULL; // list of terminated threads, using the exited pointer

thread waiting = NULL; // list of waiting threads, using the lib_one pointer

// global thread id counter
int tid_counter = 2;


// Keep track of the TID of the currently executing thread
tid_t current_running_thread_tid = 1;

// RR SCHEDULER STUFF BELOW:

void admit(thread new)
{
    /* add a thread to the pool at the tail of the linked list */

    // check if we have empty thread pool
    if (head == NULL)
    {
        head = new;
    }
    else
    {
        // loop on qlen
        thread curr_thread = head;
        while (curr_thread->sched_one != NULL)
        {
            curr_thread = curr_thread->sched_one;
        }
        // set next to new thread
        curr_thread->sched_one = new;
    }
}
void sched_remove(thread victim)
{
    /* remove a thread from the pool, should delete all references to this 
    thread unless we saved one before the call to this function */

    // check if we have empty thread pool
    if (head != NULL)
    {
        // first check if the head is what we want to remove so that we don't seg fault
        if (head->tid == victim->tid)
        {
            thread temp = head;
            head = head->sched_one;
            temp->sched_one = NULL;
        }
        else
        {
            // loop on qlen for tid
            thread prev_thread = NULL;
            thread curr_thread = head;
            int found_flag = 0;
            while (curr_thread->sched_one != NULL && !found_flag)
            {
                if (curr_thread->tid != victim->tid)
                {
                    prev_thread = curr_thread;
                    curr_thread = curr_thread->sched_one;
                }
                else
                {
                    found_flag = 1;
                }
            }
            if (curr_thread->tid == victim->tid) // need to check if the last one is the victim
            {
                found_flag = 1;
            }
            if (found_flag)
            {
                // get the next thread to link prev to next, either sets to the next thread or NULL
                
                thread next_thread = curr_thread->sched_one;
                prev_thread->sched_one = next_thread;
                curr_thread->sched_one = NULL;
            }
            else
            {   
                fprintf(stdout, "Victim: %d\n", victim->tid);
                perror("Error finding thread to remove");
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        perror("Empty queue, nothing to remove");
        exit(EXIT_FAILURE);
    }
}

thread next(void)
{
    /* select a thread to schedule   */
    // put current thread at the end of the queue, should put NULL if no other processes in pool
    if (head != NULL) {
        thread next = head;
        sched_remove(next);
        admit(next); // put the thread at the end of the queue   
        return next; // could be NULL
    }
    return NULL;
}

int qlen(void)
{
    /* number of ready threads       */
    int ready = 0;
    thread curr_thread = head;
    while (curr_thread != NULL)
    {
        ready++;
        curr_thread = curr_thread->sched_one;
    }
    return ready;
}


struct scheduler rr_publish = {NULL, NULL, admit, sched_remove, next, qlen};
scheduler RoundRobin = &rr_publish;

// END SCHEDULER FUNCTIONS

// START LWP FUNCTIONS

static void lwp_wrap(lwpfun fun, void *arg)
{
    /* call the given lwpfucntion with the given argument.
    calls lwp_exit() with its return value*/
    int rval;
    rval = fun(arg);
    lwp_exit(rval);
}

tid_t lwp_create(lwpfun function, void *argument)
{
    /*
    Creates a new thread and admits it to the current scheduler. The thread’s resources will consist of a
    context and stack, both initialized so that when the scheduler chooses this thread and its context is
    loaded via swap_rfiles() it will run the given function. This may be called by any thread.
    */
    long page_size;
    struct rlimit rlp;
    int result;
    unsigned long resource_limit;
    void *s;
    thread c;
    unsigned long *stack_pointer;

    // need to allocate memory for the context struct
    c = malloc(sizeof(context));
    if (c == NULL)
    {
        perror("Error allocating memory for context struct");
        exit(EXIT_FAILURE);
    }
    c->tid = tid_counter++;
    c->status = LWP_LIVE; 
    c->sched_one = NULL;
    c->sched_two = NULL;
    c->exited = NULL;
    c->lib_one = NULL;
    c->lib_two = NULL;

    // Determine how big the stack should be using sysconf(3)
    page_size = sysconf(_SC_PAGE_SIZE);

    // Get the value of the resource limit for the stack size (RLIMIT_STACK) using getrlimit(2). Use the soft limit
    result = getrlimit(RLIMIT_STACK, &rlp);

    // If RLIMIT_STACK does not exist or if its value is RLIM_INFINITY, use a stack size of 8MB.
    if (result == -1 || rlp.rlim_cur == RLIM_INFINITY)
    {
        resource_limit = 8 * 1024 * 1024;
    }
    else
    {
        resource_limit = rlp.rlim_cur;
        // make sure the resouce limit is a multiple of the page size, otherwise round up to the nearest multiple of the page size
        if (resource_limit % page_size != 0)
        {
            resource_limit = resource_limit + (page_size - (resource_limit % page_size));
        }
    }

    // Allocate a stack the size of our resource limit, MAP_STACK ensures stack is on 16-byte boundary
    // stack pointer will be at a low memory address
    stack_pointer = mmap(NULL, resource_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack_pointer == MAP_FAILED)
    {
        perror("Error allocating memory for stack");
        exit(EXIT_FAILURE);
    }
    if ((uintptr_t)stack_pointer % 16 != 0)
    {
        perror("Stack not properly aligned");
        exit(EXIT_FAILURE);
    }
    c->stack = stack_pointer; // Set base of the stack, need so that we can unmap later

    // now our stack pointer is at high memory address, divide by size of unsigned long
    stack_pointer = stack_pointer + (resource_limit / sizeof(unsigned long));

    // check that stack pointer is divisble by 16, move to lower addresses.
    if ((uintptr_t)stack_pointer % 16 != 0)
    {
        perror("Stack not properly aligned - after moving to lower addresses");
        exit(EXIT_FAILURE);
    }

    c->stacksize = resource_limit; // keep track of stack size in bytes

    // WE HAD TO DECREMENT LIKE THIS BECAUSE WE WERE GETTING A SEG FAULT
    stack_pointer--;
    *stack_pointer = (unsigned long)0;
    stack_pointer--;                          // this will subtract the size of an unsiged long from the stack pointer
    *stack_pointer = (unsigned long)lwp_wrap; // this will push the address of the function wrapper onto the stack
    stack_pointer--;                          // this will subtract the size of an unsiged long from the stack pointer
    // need to move the address two times so that we say alligned on 16 byte boundary

    // need to set all the registers for the new lwp using the function arguments from above
    c->state.rdi = (unsigned long)function;
    c->state.rsi = (unsigned long)argument;
    c->state.rbp = (unsigned long)stack_pointer;
    c->state.rsp = (unsigned long)stack_pointer;
    c->state.fxsave = FPU_INIT;

    // admit the context to the scheduler

    if (schedule == NULL)
    {
        schedule = RoundRobin;
    }
    schedule->admit(c);
    return c->tid;
}

void lwp_yield(void)
{ 
    //     Yields control to the next thread as indicated by the scheduler. If there is no next thread, calls exit(3)
    // with the termination status of the calling thread (see below).

    thread next_thread, current_thread;

    
    current_thread = tid2thread(lwp_gettid()); 
    next_thread = schedule->next();

    // check if next thread is null meaning we have no scheduled threads
    if(next_thread == NULL) {
        lwp_exit(3);
    }

    current_running_thread_tid = next_thread->tid;

    // swap the context of the current thread with the next thread
    swap_rfiles(&current_thread->state, &next_thread->state);
    
}


void lwp_exit(int status)
{
    // Terminates the calling thread. Its termination status becomes the low 8 bits of the passed integer. The
    // thread’s resources will be deallocated once it is waited for in lwp_wait(). Yields control to the next
    // thread using lwp_yield().
    // remove yourself from the scheduler, and change the status. Memory needs to be freed by the wait function
    // is a thread waiting? if so, admit the waiting thread back into the scheduler.
    // if no thread is waiting, add the thread to the list  of terminated theads,
    // maintiain a list of terminated and waiting threads
    // yeild at the end of this function

    thread removed_thread;
    removed_thread = tid2thread(lwp_gettid()); 
    removed_thread->status = status;
    schedule->remove(removed_thread);

    // put this thread at the end of the terminated list (exited)  // need to address the waiting threads before putting calling thread on terminated
    if (terminated == NULL)
    {   
        terminated = removed_thread;
    }
    else
    {
        thread curr_thread = terminated;
        while (curr_thread->exited != NULL)
        {
            curr_thread = curr_thread->exited;
        }
        // set next to new thread
        curr_thread->exited = removed_thread;
    }
    // check waiting list, readmit waiting threads so they can continue to clean up calling thread
    if (waiting != NULL) // if there are waiting threads, put the oldest one back into the scheduler
    {
        // get the thread at the front of the list
        thread waiting_thread = waiting;
        // remove the thread from the list
        waiting = waiting->lib_one;
        waiting_thread->lib_one = NULL;
        // admit the thread back into the scheduler
        schedule->admit(waiting_thread);
    }
    if (qlen() > 0)
    {
            lwp_yield();
    }
    thread main_calling_thread = tid2thread(1);
    swap_rfiles(&removed_thread->state, &main_calling_thread->state);
}

tid_t lwp_wait(int *status) // removing too many times somewhere before exitting, 
{
    /*Deallocates the resources of a terminated LWP. If no LWPs have terminated and there still exist
    runnable threads, blocks until one terminates. If status is non-NULL, *status is populated with its
    termination status. Returns the tid of the terminated thread or NO_THREAD if it would block forever
    because there are no more runnable threads that could terminate.*/
    thread calling_thread = tid2thread(lwp_gettid()); // USE GLOBAL NOW
    if (terminated == NULL) // no terminated threads, so we have to block
    {
        if (schedule->qlen() <= 1)        // no more runnable threads, so we would block forever
        {
            return NO_THREAD;
        }
        // add the current thread to the waiting list
        if (waiting == NULL)
        {
            waiting = calling_thread;
        }
        else // put at the back of the waiting queue
        {
            thread curr_thread = waiting;
            while (curr_thread->lib_one != NULL)
            {
                curr_thread = curr_thread->lib_one;
            }
            // set next to new thread
            curr_thread->lib_one = calling_thread;
        }
        // Yield to the next process
        thread next_thread = schedule->next();
        current_running_thread_tid = next_thread->tid;

        schedule->remove(calling_thread);
        swap_rfiles(&calling_thread->state, &next_thread->state);
    }

    // if we get here, we have a terminated thread, so we can clean up the memory
    thread terminated_thread = terminated; // get the thread at the front of the list
    // set the status of the thread, check if NULL
    if(status != NULL) {
        *status = MKTERMSTAT(LWP_TERM, terminated_thread->status);
    }
    terminated = terminated->exited;  // remove the thread from the list
    // free the memory for the stack

    munmap(terminated_thread->stack, terminated_thread->stacksize);
    free(terminated_thread); // free the memory for the context
    return terminated_thread->tid;
}

tid_t lwp_gettid(void) // problem could be here
{
    // check if we have empty thread pool
    if (schedule->qlen() == 0)
    {
        perror("No running threads, can't get tid of current thread");
        return NO_THREAD;

    }
    return current_running_thread_tid;
}

void lwp_start(void)
{
    /*Starts the threading system by converting the calling thread—the original system thread—into a LWP
    by allocating a context for it and admitting it to the scheduler, and yields control to whichever thread the
    scheduler indicates. It is not necessary to allocate a stack for this thread since it already has one.  */

    // allocate a context for the calling thread
    thread calling_thread;
    thread first_lwp;
    calling_thread = malloc(sizeof(context));
    if (calling_thread == NULL)
    {
        perror("Error allocating memory for context struct- calling thread");
        exit(EXIT_FAILURE);
    }
    calling_thread->tid = 1;
    calling_thread->status = LWP_LIVE; // thread is now live

    // admit the context to the scheduler
    schedule->admit(calling_thread);

    //  VERY LAST THING we do in this function is switch the stack to the first lwp, then when we return.
    //  We will return to lwp_wrap. To do this switch I will get the next thread from the scheduler.
    //  Then I will use swap_rfiles to switch the stack to this thread. All the info about threads will
    //  be stored in the scheduler, allowing this process to work.
    first_lwp = schedule->next(); 
    current_running_thread_tid = first_lwp->tid; 
    swap_rfiles(&calling_thread->state, &first_lwp->state);
   
}


thread tid2thread(tid_t tid)
{
    // check if we have empty thread pool
    if (head != NULL)
    {
        if (head->tid == tid)
        {
            return head;
        }
        // loop on qlen for tid
        thread curr_thread = head;
        int found_flag = 0;
        while (curr_thread->sched_one != NULL && !found_flag)
        {
            if (curr_thread->tid == tid)
            {
                found_flag = 1;
            }
            else
            {
                curr_thread = curr_thread->sched_one;
            }
        }
        if (curr_thread->tid == tid) // need to check if the last one is the victim
        {
            found_flag = 1;
        }
        if (found_flag)
        {
            return curr_thread;
        }
    }
    if (terminated != NULL)
    {
        if (terminated->tid == tid)
        {
            return terminated;
        }
        // loop on qlen for tid
        thread curr_thread = terminated;
        int found_flag = 0;
        while (curr_thread->exited != NULL && !found_flag)
        {
            if (curr_thread->tid == tid)
            {
                found_flag = 1;
            }
            else
            {
                curr_thread = curr_thread->exited;
            }
        }
        if (curr_thread->tid == tid) // need to check if the last one is the victim
        {
            found_flag = 1;
        }
        if (found_flag)
        {
            return curr_thread;
        }
    }
    if (waiting != NULL)
    {
        if (waiting->tid == tid)
        {
            return waiting;
        }
        // loop on qlen for tid
        thread curr_thread = waiting;
        int found_flag = 0;
        while (curr_thread->lib_one != NULL && !found_flag)
        {
            if (curr_thread->tid == tid)
            {
                found_flag = 1;
            }
            else
            {
                curr_thread = curr_thread->lib_one;
            }
        }
        if (curr_thread->tid == tid) // need to check if the last one is the victim
        {
            found_flag = 1;
        }
        if (found_flag)
        {
            return curr_thread;
        }
    }
    return NULL;
}

void lwp_set_scheduler(scheduler fun)
{
    // if fun is null initialize round robin
    if (fun != NULL)
    {
        schedule = fun;
        fun->init();
    }
    else
    {
        schedule = RoundRobin;
    }
}

scheduler lwp_get_scheduler(void)
{
    return schedule;
}