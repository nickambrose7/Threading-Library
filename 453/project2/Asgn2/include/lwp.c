#include <lwp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/mman.h>

// define global variables
// Need to keep track of the return address that we replaced with the address of the function arg
// need to keep track of the scheduler
int tid_counter;
tid_counter = 1;


tid_t lwp_create(lwpfun function, void *argument) {
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
    context *c;
    rfile *new_rfile;
    tid_counter++;

    // // addition... making new thread
    // tid_t newthread;
    // //initializing FPU for regfile of thread
    // newthread->state.fxsave=FPU_INIT;

    // need to allocate memory for the context struct
    c = malloc(sizeof(context));
    if (c == NULL)
    {
        perror("Error allocating memory for context struct");
        exit(EXIT_FAILURE);
    }
    c->tid = tid_counter;
    c->status = 0; // not running yet

    // need to allocate memory for the rfile structs, one for the new thread and one for the old thread
    new_rfile = malloc(sizeof(rfile));
    if (new_rfile == NULL)
    {
        perror("Error allocating memory for rfile struct");
        exit(EXIT_FAILURE);
    }

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
    // need to admit the context to the scheduler at some point too

    // Allocate a stack the size of our resource limit, MAP_STACK ensures stack is on 16-byte boundary
    // stack pointer will be at a low memory address
    stack_pointer = mmap(NULL, resource_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack_pointer == MAP_FAILED)
    {
        perror("Error allocating memory for stack");
        exit(EXIT_FAILURE);
    }
    stack_pointer = stack_pointer + resource_limit; // now our stack pointer is at high memory address
    c->stack = stack_pointer;
    c->stacksize = resource_limit;

    // need to push the address of the function wrapper onto the stack
    *stack_pointer = lwp_wrap; // this will push the address of the function wrapper onto the stack
    stack_pointer--;           // this will subtract the size of an unsiged long from the stack pointer

    // need to set all the registers for the new lwp using the function arguments from above
    // put the the arg pointer goes into the register %rdi
    new_rfile->rdi = function;
    new_rfile->rsi = argument;
    new_rfile->rbp = stack_pointer;
    new_rfile->rsp = stack_pointer;
    new_rfile->fxsave = FPU_INIT;
    // Do I need to set the registers to 0?
}

void lwp_start()
{
    /*
Starts the threading system by converting the calling thread—the original system thread—into a LWP
by allocating a context for it and admitting it to the scheduler, and yields control to whichever thread the
scheduler indicates. It is not necessary to allocate a stack for this thread since it already has one.*/
}

static void lwp_wrap(lwpfun fun, void *arg)
{
    /* call the given lwpfucntion with the given argument.
    calls lwp_exit() with its return value*/
    int rval;
    rval = fun(arg);
    lwp_exit(rval);
}

void lwp_yield() {
    /*Yields control to the next thread as indicated by the scheduler. If there is no next thread, calls exit(3)
    with the termination status of the calling thread*/

    // get next thread from scheduler
    thread next_thread = scheduler.next();
    // if not null switch context and return address
    if (next_thread != NULL) {

    }

    // else exit(LWP_TERM)
    else {

    }
}

void  lwp_exit(int status) {
    /*Starts the threading system by converting the calling thread—the original system thread—into a LWP
    by allocating a context for it and admitting it to the scheduler, and yields control to whichever thread the
    scheduler indicates. It is not necessary to allocate a stack for this thread since it already has one.*/

}

tid_t lwp_gettid(void) {

}

void  lwp_start(void) {
    /*Starts the threading system by converting the calling thread—the original system thread—into a LWP
    by allocating a context for it and admitting it to the scheduler, and yields control to whichever thread the
    scheduler indicates. It is not necessary to allocate a stack for this thread since it already has one.  */

}

tid_t lwp_wait(int *){
    /*Deallocates the resources of a terminated LWP. If no LWPs have terminated and there still exist
    runnable threads, blocks until one terminates. If status is non-NULL, *status is populated with its
    termination status. Returns the tid of the terminated thread or NO_THREAD if it would block forever
    because there are no more runnable threads that could terminate.*/
}

void  lwp_set_scheduler(scheduler fun) {

}

scheduler lwp_get_scheduler(void) {

}

thread tid2thread(tid_t tid) {

}