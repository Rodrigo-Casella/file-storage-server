#include "../include/define_source.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include "../include/utils.h"

#define DFL_SOCKET "./mysock"
#define DFL_BACKLOG 50

int hardQuit, softQuit;

int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}

void cleanup()
{
    unlink(DFL_SOCKET);
}

void server_shutdown_handler(int signum)
{
    if (signum == SIGHUP)
        softQuit = 1;
    else
        hardQuit = 1;
}

int main(int argc, char const *argv[])
{
    atexit(cleanup);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = server_shutdown_handler;

    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGINT, &act, NULL);
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGQUIT, &act, NULL);
    SYSCALL_EQ_ACTION(sigaction, -1, exit(EXIT_FAILURE), SIGHUP, &act, NULL);

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    strncpy(server_addr.sun_path, DFL_SOCKET, UNIX_PATH_MAX);
    server_addr.sun_family = AF_UNIX;

    int listen_fd, fd_max = 0;

    fd_set set, rdset;

    int connected_clients = 0;
    
    hardQuit = 0, softQuit = 0;

    SYSCALL_RET_EQ_ACTION(socket, -1, listen_fd, exit(EXIT_FAILURE), AF_UNIX, SOCK_STREAM, 0);

    fd_max = MAX(fd_max, listen_fd);
    FD_ZERO(&set);
    FD_SET(listen_fd, &set);

    SYSCALL_EQ_ACTION(bind, -1, exit(EXIT_FAILURE), listen_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    SYSCALL_EQ_ACTION(listen, -1, exit(EXIT_FAILURE), listen_fd, SOMAXCONN);

    while (!hardQuit)
    {
        
        rdset = set;

        if(select(fd_max + 1, &rdset, NULL, NULL, NULL) == -1)
        {
            if (errno == EINTR)
            {
                if (connected_clients == 0 && softQuit)
                    break;
                
                continue;
            }
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int fd = 0; fd <= fd_max; fd++)
        {
            int fd_c;
            if (FD_ISSET(fd, &set))
            {
                
                if (fd == listen_fd)
                {
                    SYSCALL_RET_EQ_ACTION(accept, -1, fd_c, continue, listen_fd, NULL, 0);
                    printf("Client %d connesso\n", fd_c);

                    if(softQuit)
                    {
                        SYSCALL_EQ_ACTION(close, -1, continue, fd_c);
                        printf("Client %d disconnesso\n", fd_c);
                    }
                    /*FD_SET(fd_c, &set);
                    fd_max = MAX(fd_max, fd_c);*/
                    SYSCALL_EQ_ACTION(close, -1, continue, fd_c);
                    printf("Client %d disconnesso\n", fd_c);
                }
            }
        }
        
    }

    SYSCALL_EQ_ACTION(close, -1, exit(EXIT_FAILURE), listen_fd);
    printf("\nChiudendo il server\n");
    return 0;
}