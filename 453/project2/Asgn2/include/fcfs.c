#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "fcfs.h"
#include "rr.h"
#include "lwp.h"
#include "thread_list.h"


scheduler FirstComeFirstServe = &(struct scheduler){ fcfs_init, fcfs_shutdown, fcfs_admit, fcfs_remove, fcfs_next, fcfs_qlen };

static thread_list *container = NULL;

void fcfs_init(void) {
  if (container) {
    return;
  }

  container = create_thread_list();
  if (!container) {
    exit(1);
  }
}

void fcfs_shutdown(void) {
  if (!container) {
    return;
  }

  free_thread_list(container);
}

void fcfs_admit(thread new) {
  if (!container) {
    return;
  }

  append_thread_list(container, new);
}

void fcfs_remove(thread victim) {
  if (!container) {
    return;
  }

  remove_from_thread_list_by_tid(container, victim->tid);
}

thread fcfs_next(void) {
  if (!container) {
    return NULL;
  }

  return container->root->value;
}

int fcfs_qlen(void) {
  if (!container) {
    return NO_THREAD;
  }

  int thread_count = 0;

  thread_list_node *cursor = container->root;
  while (cursor) {
    if (!cursor->value) {
        cursor = cursor->next;
        continue;
    }

    // do not include terminated threads in the count
    if (LWPTERMINATED(cursor->value->status)) {
      cursor = cursor->next;
      continue;
    }

    thread_count++;
    cursor = cursor->next;
  }

  return thread_count;
}