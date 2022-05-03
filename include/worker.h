#ifndef WORKER_H
#define WORKER_H

#include "../include/boundedqueue.h"

typedef struct threadArgs
{
    BQueue_t *queue;
    int write_end_pipe_fd;
} ThreadArgs;

void *processRequest(void *args);
#endif