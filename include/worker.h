#ifndef WORKER_H
#define WORKER_H

#include "../include/boundedqueue.h"
#include "../include/filesystem.h"

typedef struct threadArgs
{
    BQueue_t *queue;
    Filesystem *fs;
    int write_end_pipe_fd;
} ThreadArgs;

void *processRequest(void *args);
#endif