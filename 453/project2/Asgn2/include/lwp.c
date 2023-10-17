#include <lwp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>


// define global variables
// Need to keep track of the return address that we replaced with the address of the function arg
// need to keep track of the scheduler
int tid_counter;
tid_counter = 0;


tid lwp_create(lwpfun function, void *argument) {
    /*
    Creates a new thread and admits it to the current scheduler. The thread’s resources will consist of a
context and stack, both initialized so that when the scheduler chooses this thread and its context is
loaded via swap_rfiles() it will run the given function. This may be called by any thread.
*/
    long page_size;
    struct rlimit rlp;
    int result
    rlim_t resource_limit;
    void *s;
    context *c;
    rfile *r;
    tid_counter++;

    // need to allocate memory for the context struct
    c = malloc(sizeof(context));
    if (c == NULL) {
        perror("Error allocating memory for context struct");
        exit(EXIT_FAILURE);
    }
    c->tid = tid_counter;
    c->status = 0; // not running yet

    // need to allocate memory for the rfile struct
    r = malloc(sizeof(rfile));
    if (r == NULL) {
        perror("Error allocating memory for rfile struct");
        exit(EXIT_FAILURE);
    }

    //Determine how big the stack should be using sysconf(3)
    page_size = sysconf(_SC_PAGE_SIZE);

    // Get the value of the resource limit for the stack size (RLIMIT_STACK) using getrlimit(2). Use the soft limit
    result = getrlimit(RLIMIT_STACK, &rlp);
    //If RLIMIT_STACK does not exist or if its value is RLIM_INFINITY, use a stack size of 8MB.
    if (result == -1 || rlp.rlim_cur == RLIM_INFINITY) {
        resource_limit = 8 * 1024 * 1024;
    } else {
        resource_limit = rlp.rlim_cur;
        // make sure the resouce limit is a multiple of the page size, otherwise round up to the nearest multiple of the page size
        if (resource_limit % page_size != 0) {
            resource_limit = resource_limit + (page_size - (resource_limit % page_size));
        }
    }
    // need to admit the context to the scheduler at some point too
    
    // Allocate a stack the size of our resource limit
   c->stack = mmap(NULL, resource_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
   c->stacksize = resource_limit;
   // need to push the address of the function onto the stack
}

