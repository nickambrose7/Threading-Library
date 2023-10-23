#include "lwp.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#define _GNU_SOURCE
#include <sys/mman.h>

// define global variables

// making round robin scheduler
// do not need init and shutdown because our structure is just a thread struct
struct scheduler rr_publish = {NULL, NULL, admit, remove, next, qlen};
scheduler RoundRobin = &rr_publish;

// global scheduler
scheduler schedule;
// global threadpool --> make thread pointer, maybe static
thread head = NULL; // head of the thread pool, using the sched_one pointer

thread terminated = NULL; // list of terminated threads, using the exited pointer

thread waiting = NULL; // list of waiting threads, using the sched_two pointer

// Need to keep track of the return address that we replaced with the address of the function arg
// need to keep track of the scheduler
int tid_counter = 2;
scheduler sched;

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
    rlim_t resource_limit;
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
    c->status = 0; // not running yet

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

    // need to push the address of the function wrapper onto the stack
    stack_pointer--;           // this will subtract the size of an unsiged long from the stack pointer
    *stack_pointer = lwp_wrap; // this will push the address of the function wrapper onto the stack
    stack_pointer--;           // this will subtract the size of an unsiged long from the stack pointer
    // need to move the address two times so that we say alligned on 16 byte boundary

    // need to set all the registers for the new lwp using the function arguments from above
    // put the the arg pointer goes into the register %rdi
    c->state.rdi = function;
    c->state.rsi = argument;
    c->state.rbp = stack_pointer; // should this go before we add the addres to the stack?
    c->state.rsp = stack_pointer;
    c->state.fxsave = FPU_INIT;

    // admit the context to the scheduler
    sched->admit(c);

    return c->tid;
}

void lwp_yield(void)
{ // CHECK CORRECTNESS WITH TEACHER
    //     Yields control to the next thread as indicated by the scheduler. If there is no next thread, calls exit(3)
    // with the termination status of the calling thread (see below).

    // TODO: get the next thread from the scheduler
    tid_t current_tid;
    thread next_thread, current_thread;
    current_tid = lwp_gettid();
    current_thread = tid2thread(current_tid);
    next_thread = sched->next();

    // TODO: admit the old thread to the scheduler
    sched->admit(current_thread);

    // TODO: swap the context of the current thread with the next thread
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
    sched->remove(removed_thread);

    // put this thread at the end of the terminated list (exited)
    if (terminated == NULL)
    {
        terminated = removed_thread;
    }
    else
    {
        // loop on qlen
        thread curr_thread = terminated;
        while (curr_thread->exited != NULL)
        {
            curr_thread = curr_thread->exited;
        }
        // set next to new thread
        curr_thread->exited = removed_thread;
    }

    lwp_yield();
}

tid_t lwp_gettid(void)
{ // CHECK CORRECTNESS WITH TEACHER
    // check if we have empty thread pool
    int thread_id = 0;
    if (head != NULL)
    {
        // loop on qlen for tid
        thread curr_thread = head;
        while (curr_thread->sched_one != NULL && thread_id == 0)
        {
            if (curr_thread->tid == 1)
            {
                thread_id = 1;
            }
            else
            {
                curr_thread = curr_thread->sched_one;
            }
        }

        if (thread_id == 0)
        {
            return NO_THREAD;
        }
    }
    return thread_id;
}

void lwp_start(void)
{
    /*Starts the threading system by converting the calling thread—the original system thread—into a LWP
    by allocating a context for it and admitting it to the scheduler, and yields control to whichever thread the
    scheduler indicates. It is not necessary to allocate a stack for this thread since it already has one.  */

    // TODO: allocate a context for the calling thread
    thread calling_thread;
    thread first_lwp;
    calling_thread = malloc(sizeof(context));
    if (calling_thread == NULL)
    {
        perror("Error allocating memory for context struct- calling thread");
        exit(EXIT_FAILURE);
    }
    calling_thread->tid = 1;
    calling_thread->status = 0; // not running yet

    // admit the context to the scheduler
    sched->admit(calling_thread);

    // TODO: VERY LAST THING we do in this function is switch the stack to the first lwp, then when we return
    //  we will return to lwp_wrap. To do this switch I will get the next thread from the scheduler.
    //  Then I will use swap_rfiles to switch the stack to this thread. All the info about threads will
    //  be stored in the scheduler, allowing this process to work.
    first_lwp = sched->next();
    swap_rfiles(&calling_thread->state, &first_lwp->state);
}

tid_t lwp_wait(int *status)
{
    /*Deallocates the resources of a terminated LWP. If no LWPs have terminated and there still exist
    runnable threads, blocks until one terminates. If status is non-NULL, *status is populated with its
    termination status. Returns the tid of the terminated thread or NO_THREAD if it would block forever
    because there are no more runnable threads that could terminate.*/

    if (terminated == NULL) // no terminated threads, so we have to block
    {
        sched->remove(tid2thread(lwp_gettid())); // deschedule the current thread, to block
        // add the current thread to the waiting list
        if (waiting == NULL)
        {
            waiting = tid2thread(lwp_gettid());
        }
        else
        {
            // loop on qlen
            thread curr_thread = waiting;
            while (curr_thread->sched_two != NULL)
            {
                curr_thread = curr_thread->sched_two;
            }
            // set next to new thread
            curr_thread->sched_two = tid2thread(lwp_gettid());
        }
    }
    else // return the thread at the front of the list, this is the oldest one
    {
        // get the thread at the front of the list
        thread terminated_thread = terminated;
        // remove the thread from the list
        terminated = terminated->exited;
        // free the memory for the stack
        munmap(terminated_thread->stack, terminated_thread->stacksize);
        // free the memory for the context
        free(terminated_thread);
        // return the tid of the terminated thread
        return terminated_thread->tid;
    }
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

thread tid2thread(tid_t tid)
{
    // check if we have empty thread pool
    if (head != NULL)
    {
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

        if (found_flag)
        {
            return curr_thread;
        }
    }
    return NULL;
}

// void init(void) {
//     /* initialize any structures     */
// }

// void shutdown(void){
//     /* tear down any structures      */
//     free(threadPool);
// }

void admit(thread new)
{
    /* add a thread to the pool      */

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
void remove(thread victim)
{
    /* remove a thread from the pool */

    // check if we have empty thread pool
    if (head != NULL)
    {
        // loop on qlen for tid
        thread prev_thread = head;
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

        if (found_flag)
        {
            // get the next thread to link prev to next, either sets to the next thread or NULL
            thread next_thread = curr_thread->sched_one;
            // // check the next thread
            // if (curr_thread->sched_one != NULL) {
            //     // set next to new thread
            prev_thread->sched_one = next_thread;
            // }
            // else {
            //     prev_thread->sched_one = NULL;
            // }
        }
        else
        {
            perror("Error finding thread to remove");
            exit(EXIT_FAILURE);
        }
    }
}

thread next(void)
{
    /* select a thread to schedule   */
    // QUESTION: Maybe the waiting for yielded process and then kick out next here or in wait?
    // waiting for the current thread to yield; may have to approach differently
    while (head->status != LWP_TERM)
        ;
    // put current thread at the end of the queue, should put NULL if no other processes in pool?
    thread finished = head;
    head = head->sched_one;
    finished->sched_one = NULL;
    admit(finished);
    // return the next thread
    return head;
}

int qlen(void)
{
    /* number of ready threads       */
    // is the readiness of a thread stored in status? loops
    // through queue and check ready status or return qlen?
    int ready = 0;
    thread curr_thread = head;
    while (curr_thread != NULL && curr_thread->status == LWP_LIVE)
    {
        ready++;
        curr_thread = curr_thread->sched_one;
    }
    return ready;
}
