#ifndef FCFS_H
#define FCFS_H

#include "lwp.h"

extern scheduler FirstComeFirstServe;

void fcfs_init(void);
void fcfs_shutdown(void);
void fcfs_admit(thread new);
void fcfs_remove(thread victim);
thread fcfs_next(void);
int fcfs_qlen(void);

#endif