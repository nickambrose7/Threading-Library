#ifndef THREAD_LIST_H
#define THREAD_LIST_H

#include "lwp.h"

typedef struct thread_list_node {
    thread value;
    struct thread_list_node *next;
} thread_list_node;

typedef struct thread_list {
    thread_list_node *root;
    thread_list_node *active;
} thread_list;

thread_list *create_thread_list();
void append_thread_list(thread_list *list, thread thread);
thread shift_thread_list(thread_list *list);
thread remove_from_thread_list_by_tid(thread_list *list, tid_t tid);
size_t thread_list_size(thread_list *list);
void free_thread_list(thread_list *list);

#endif