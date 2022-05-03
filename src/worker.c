#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>

#include "../include/worker.h"
#include "../include/utils.h"

#define THREAD_ERR_EXIT                                       \
    fprintf(stderr, "Errore in thread: %ld", pthread_self()); \
    pthread_exit(NULL);

void *processRequest(void *args)
{
    BQueue_t *client_request_queue = ((ThreadArgs *)args)->queue;
    int managerFd = ((ThreadArgs *)args)->write_end_pipe_fd;

    while (1)
    {
        int *client_fd = pop(client_request_queue);

        if (client_fd == EOS)
            break;

        printf("thread: %ld, client: %d\n", pthread_self(), *client_fd);

        CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, "0", strlen("0") + 1);

        SYSCALL_EQ_ACTION(close, -1, THREAD_ERR_EXIT, *client_fd);
        printf("client: %d disconesso\n", *client_fd);

        free(client_fd);
    }

    return NULL;
}