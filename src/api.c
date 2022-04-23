#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>

#include "../include/api.h"
#include "../include/utils.h"

int toPrint = 0;
int fd_skt = 0;

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
    SYSCALL_RET_EQ_ACTION(socket, -1, fd_skt, return -1, AF_UNIX, SOCK_STREAM, 0);
    struct timespec retry_time;
    setTimespecMsec(&retry_time, msec);

    struct sockaddr_un server_addr;
    strncpy(server_addr.sun_path, sockname, UNIX_PATH_MAX);
    server_addr.sun_family = AF_UNIX;
    
    while (connect(fd_skt, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
    {
        printf("fd_skt %d\n", fd_skt);
        if (errno != ENOENT && errno != EAGAIN)
            return -1;

        if (toPrint)
            printf("Ritento la connessione tra %d msec\n", msec);

        nanosleep(&retry_time, NULL);

        if (time(NULL) >= abstime.tv_sec)
        {
            if (toPrint)
                printf("Tempo scaduto\n");

            return -1;
        }    
    }
    if (toPrint)
        printf("Connessione al server riuscita\n");

    return 0;
}