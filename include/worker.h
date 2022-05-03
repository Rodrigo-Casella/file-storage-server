#ifndef WORKER_H
#define WORKER_H

#include "../include/boundedqueue.h"

typedef struct threadArgs
{
    BQueue_t *queue;
} ThreadArgs;

void *processRequest(void *args);
#endif