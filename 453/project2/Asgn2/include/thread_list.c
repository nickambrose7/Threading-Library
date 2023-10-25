#include "thread_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>


thread_list *create_thread_list() {
    thread_list *list = malloc(sizeof(thread_list));
    if (!list) {
        perror("failed to allocate thread_list");
    }

    list->root = NULL;
    list->active = NULL;

    return list;
}

void append_thread_list(thread_list *list, thread thread) {
    if (!list) {
        return;
    }

    thread_list_node *node = malloc(sizeof(thread_list_node));
    if (!node) {
        perror("failed to allocate thread_list_node");
        free_thread_list(list);
        return;
    }

    node->value = thread;
    node->next = NULL;

    if (!list->root) {
        list->root = node;
    } else {
        thread_list_node *cursor = list->root;
        while (cursor->next) {
            cursor = cursor->next;
        }

        cursor->next = node;
    }
}

thread shift_thread_list(thread_list *list) {
    if (!list) {
        return NULL;
    }

    if (list->root) {
        thread_list_node *ret = list->root;
        thread retval = ret->value;
        list->root = list->root->next;
        free(ret);
        return retval;
    }

    return NULL;
}

thread remove_from_thread_list_by_tid(thread_list *list, tid_t tid) {
    if (!list) {
        //fprintf(stdout, "Seg fault!\n");
        return NULL;
    }

    thread_list_node *cursor = list->root;
    thread_list_node *prev = NULL;
    thread ret = NULL;

    while (cursor) {
        if (!cursor->value) {
            continue;
        }

        if (cursor->value->tid == tid) {
            if (cursor->next) {
                if (prev) {
                    prev->next = cursor->next;
                } else {
                    list->root = cursor->next;
                }
            } else {
                if (prev) {
                    prev->next = NULL;
                } else {
                    list->root = NULL;
                }
            }

            ret = cursor->value;
            free(cursor);
            break;
        }

        prev = cursor;
        cursor = cursor->next;
    }

    return ret;
}

size_t thread_list_size(thread_list *list) {
    if (!list) {
        return 0;
    }

    size_t size = 0;
    
    thread_list_node *cursor = list->root;
    while (cursor) {
        size++;
        cursor = cursor->next;
    }

    return size;
}

void free_thread_list(thread_list *list) {
    if (!list) {
        return;
    }

    thread_list_node *cursor = list->root;
    while (cursor) {
        thread_list_node *next = cursor->next;
        
        if (cursor->value) {
            if (cursor->value->lib_one) {
                munmap(cursor->value->stack, cursor->value->stacksize);
            }

            free(cursor->value);
            free(cursor);
        }
        
        cursor = next;
    }
    
    free(list);
}