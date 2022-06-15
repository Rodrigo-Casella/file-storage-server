#include "../include/define_source.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "../include/api.h"
#include "../include/message_protocol.h"
#include "../include/utils.h"

#define CHUNK_SIZE 1024

const char *responseMsg[] = {
    "Ok\n",
    "Richiesta invalida\n",
    "Il file non esiste\n",
    "Il file esiste di gia'\n",
    "Errore del server\n",
    "File in stato di lock\n",
    "File troppo grande per essere salvato sul server\n",
    "Risposta invalida dal server\n"};

#define PRINT_OP(op, file, outcome) \
    printf("%s: %s %s", #op, file, responseMsg[outcome - 1]);

#define SERVER_RESPONSE(op, file)                                   \
    if (1)                                                          \
    {                                                               \
        int response_code;                                      \
        if (read(fd_skt, &response_code, sizeof(int)) == -1)    \
        {                                                           \
            PRINT_OP(op, file, INVALID_RES);                        \
            return -1;                                              \
        }                                                           \
        if (response_code < SUCCESS || response_code > INVALID_RES) \
        {                                                           \
            PRINT_OP(op, file, INVALID_RES);                        \
            errno = EBADE;                                          \
            return -1;                                              \
        }                                                           \
        if (toPrint)                                                \
            PRINT_OP(op, file, response_code);                      \
                                                                    \
        errno = response_code != SUCCESS ? EBADE : 0;               \
    }

#define PRINT_RDWR_BYTES(bytes, op)           \
    if (toPrint && !errno)                    \
    {                                         \
        printf("%ld bytes %s\n", bytes, #op); \
    }

int toPrint = 0;
int fd_skt = 0;
char server_addr_path[UNIX_PATH_MAX];

static int readFileFromPath(const char *path, char **file_data, size_t *file_len)
{
    int file_fd;

    if ((file_fd = open(path, O_RDONLY)) == -1)
        return -1;

    if ((*file_len = lseek(file_fd, 0L, SEEK_END)) == -1)
        return -1;

    if (lseek(file_fd, 0L, SEEK_SET) == -1)
        return -1;

    *file_data = calloc((*file_len), sizeof(char));

    if (!(*file_data))
        return -1;


    if (readn(file_fd, *file_data, *file_len) == -1)
            return -1;

    if (close(file_fd) == -1)
        return -1;

    return 0;
}

static int buildRequest(struct iovec request[], size_t request_len, int *op, size_t *request_msg_len, char *request_msg)
{
    if (request_len < 3 || (*op < OPEN_FILE || *op > CLOSE_CONNECTION) || !request_msg || *request_msg_len < 1)
    {
        errno = EINVAL;
        return -1;
    }

    request[0].iov_base = op;
    request[0].iov_len = sizeof(int);

    request[1].iov_base = request_msg_len;
    request[1].iov_len = sizeof(size_t);

    request[2].iov_base = request_msg;
    request[2].iov_len = *request_msg_len;

    return 0;
}

int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
    if (!sockname || msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    SYSCALL_RET_EQ_ACTION(socket, -1, fd_skt, return -1, AF_UNIX, SOCK_STREAM, 0);
    struct timespec retry_time;
    setTimespecMsec(&retry_time, msec);

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    strncpy(server_addr.sun_path, sockname, UNIX_PATH_MAX);
    server_addr.sun_family = AF_UNIX;

    while (connect(fd_skt, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        if (errno != ENOENT && errno != EAGAIN)
            return -1;

        if (toPrint)
            printf("Ritento la connessione tra %d msec\n", msec);

        nanosleep(&retry_time, NULL);

        if (time(NULL) >= abstime.tv_sec)
        {
            if (toPrint)
                printf("Tempo scaduto\n");

            errno = ETIME;
            return -1;
        }
    }
    if (toPrint)
        printf("Connessione al server riuscita\n");

    strncpy(server_addr_path, sockname, UNIX_PATH_MAX);
    return 0;
}

int closeConnection(const char *sockname)
{
    if (!sockname || strncmp(sockname, server_addr_path, UNIX_PATH_MAX) != 0)
    {
        errno = EINVAL;
        return -1;
    }

    int op = CLOSE_CONNECTION;
    char left_msg[] = "0";
    size_t left_msg_len = ARRAY_SIZE(left_msg);

    struct iovec request[3];

    if (buildRequest(request, ARRAY_SIZE(request), &op, &left_msg_len, left_msg) == -1)
        return -1;

    if (writev(fd_skt, request, ARRAY_SIZE(request)) == -1)
        return -1;

    SYSCALL_EQ_ACTION(close, -1, return -1, fd_skt);

    if (toPrint)
        puts("Disconesso dal server");

    return 0;
}

int openFile(const char *pathname, int flags)
{
    char *pathname_buf;

    int op;

    size_t pathname_len;

    struct iovec request[4];

    if (!pathname || (pathname_len = strlen(pathname)) < 1 || flags < 0)
    {
        errno = EINVAL;
        return -1;
    }

    op = OPEN_FILE;
    pathname_buf = calloc(++pathname_len, sizeof(char)); // alloco memoria per la stringa + '\0'

    if (!pathname_buf)
        return -1;

    strncpy(pathname_buf, pathname, pathname_len);

    if (buildRequest(request, ARRAY_SIZE(request), &op, &pathname_len, pathname_buf) == -1)
        return -1;

    request[3].iov_base = &flags;
    request[3].iov_len = sizeof(int);

    if (writev(fd_skt, request, ARRAY_SIZE(request)) == -1)
        return -1;

    free(pathname_buf);

    SERVER_RESPONSE(openFile, pathname);

    return errno ? -1 : 0;
}

int writeFile(const char *pathname, const char *dirname)
{
    char *pathname_buf,
        *file_data_buf;

    int op;

    size_t pathname_len,
        file_len;

    struct iovec request[5];

    if (!pathname || (pathname_len = strlen(pathname)) < 0)
    {
        errno = EINVAL;
        return -1;
    }

    op = WRITE_FILE;

    pathname_buf = calloc(++pathname_len, sizeof(char));

    if (!pathname_buf)
        return -1;

    strncpy(pathname_buf, pathname, pathname_len);

    if (readFileFromPath(pathname, &file_data_buf, &file_len) == -1)
        return -1;

    if (buildRequest(request, ARRAY_SIZE(request), &op, &pathname_len, pathname_buf) == -1)
        return -1;

    request[3].iov_base = &file_len;
    request[3].iov_len = sizeof(size_t);

    request[4].iov_base = file_data_buf;
    request[4].iov_len = file_len;

    if (writev(fd_skt, request, ARRAY_SIZE(request)) == -1)
    {
        perror("writev");
        return -1;
    }

    free(pathname_buf);
    free(file_data_buf);

    SERVER_RESPONSE(writeFile, pathname);

    PRINT_RDWR_BYTES(file_len, scritti);
    return errno ? -1 : 0;
}

int closeFile(const char *pathname)
{
    char *pathname_buf;

    int op;

    size_t pathname_len;

    struct iovec request[3];

    if (!pathname || (pathname_len = strlen(pathname)) < 1)
    {
        errno = EINVAL;
        return -1;
    }

    op = CLOSE_FILE;

    pathname_buf = calloc(++pathname_len, sizeof(char));

    if (!pathname_buf)
        return -1;

    strncpy(pathname_buf, pathname, pathname_len);

    if (buildRequest(request, ARRAY_SIZE(request), &op, &pathname_len, pathname_buf) == -1)
        return -1;

    if (writev(fd_skt, request, ARRAY_SIZE(request)) == -1)
    {
        perror("writev");
        return -1;
    }

    free(pathname_buf);

    SERVER_RESPONSE(closeFile, pathname);

    return errno ? -1 : 0;
}