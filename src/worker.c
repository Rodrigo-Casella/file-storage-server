#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>

#include "../include/worker.h"
#include "../include/utils.h"

void *processRequest(void *args)
{
    BQueue_t *client_request_queue = ((ThreadArgs*) args)->queue;

    while (1)
    {
        int *client_fd = pop(client_request_queue);

        if (client_fd == EOS) break;
    }

    return NULL;
}