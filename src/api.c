#include "../include/define_source.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "../include/api.h"
#include "../include/message_protocol.h"
#include "../include/utils.h"

#define BUF_SIZE 1024

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

#define SERVER_RESPONSE(op, file)                       \
    if (1)                                              \
    {                                                   \
        char response[RES_CODE_LEN + 1] = "";           \
        if (read(fd_skt, response, RES_CODE_LEN) == -1) \
            return -1;                                  \
                                                        \
        long outcome = 0;                               \
        if (isNumber(response, &outcome) != 0)          \
        {                                               \
            PRINT_OP(op, file, INVALID_RES);            \
            errno = EINVAL;                             \
            return -1;                                  \
        }                                               \
        if (toPrint)                                    \
            PRINT_OP(op, file, outcome);                \
                                                        \
        errno = outcome != SUCCESS ? EBADE : 0;         \
    }

#define PRINT_RDWR_BYTES(bytes, op) \
if(toPrint) { \
    fprintf(stdout, "%ld bytes %s\n", bytes, #op); \
}

int toPrint = 0;
int fd_skt = 0;
char server_addr_path[UNIX_PATH_MAX];

static int readFileFromPath(const char *path, void **file_data, size_t *file_len)
{
    int file_fd;
    char buf[BUF_SIZE];
    char *file_data_ptr;
    int bytes_read;
    size_t bytes_copied;

    if ((file_fd = open(path, O_RDONLY)) == -1)
        return -1;

    if ((*file_len = lseek(file_fd, 0L, SEEK_END)) == -1)
        return -1;

    if (lseek(file_fd, 0L, SEEK_SET) == -1)
        return -1;

    *file_data = calloc((*file_len) + 1, 1);

    if (!file_data)
        return -1;

    file_data_ptr = (char *) *file_data;
    bytes_copied = 0;

    while (bytes_copied < (*file_len))
    {
        if ((bytes_read = readn(file_fd, buf, BUF_SIZE)) == -1)
            return -1;

        
        memcpy(file_data_ptr + bytes_copied, buf, bytes_read);
        bytes_copied += bytes_read;
    }

    close(file_fd);

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

    SYSCALL_EQ_ACTION(close, -1, return -1, fd_skt);

    if (toPrint)
        puts("Disconesso dal server");

    return 0;
}

int openFile(const char *pathname, int flags)
{
    size_t pathname_length;

    if (!pathname || (pathname_length = strlen(pathname)) < 1)
    {
        errno = EINVAL;
        return -1;
    }

    size_t request_len = REQ_CODE_LEN + MAX_SEG_LEN + pathname_length + 1 + OPEN_FLAG_LEN;
    char *request = calloc(request_len, sizeof(char));

    if (!request)
        return -1;

    snprintf(request, request_len, "%d%010ld%s%d", OPEN_FILE, pathname_length, pathname, flags);

    if (writen(fd_skt, request, request_len - 1) == -1)
        return -1;

    free(request);

    SERVER_RESPONSE(openFile, pathname);

    return errno ? -1 : 0;
}

int writeFile(const char *pathname, const char *dirname)
{
    size_t pathname_len = 0;

    if (!pathname || (pathname_len = strlen(pathname)) < 0)
    {
        errno = EINVAL;
        return -1;
    }

    void *file_data_buf = NULL;
    size_t file_len = 0;

    if (readFileFromPath(pathname, &file_data_buf, &file_len) == -1)
        return -1;

    size_t request_len = REQ_CODE_LEN + MAX_SEG_LEN + pathname_len + 1 + MAX_SEG_LEN + file_len;
    char *request = calloc(request_len, sizeof(char));

    if (!request)
        return -1;

    snprintf(request, request_len, "%d%010ld%s%010ld", WRITE_FILE, pathname_len, pathname, file_len);
    memcpy((request + strlen(request)), file_data_buf, file_len);

    if (writen(fd_skt, request, request_len - 1) == -1)
        return -1;

    free(file_data_buf);
    free(request);

    SERVER_RESPONSE(writeFile, pathname);
    PRINT_RDWR_BYTES(file_len, scritti);
    return errno ? -1 : 0;
}

int closeFile(const char *pathname)
{
    size_t pathname_length;

    if (!pathname || (pathname_length = strlen(pathname)) < 1)
    {
        errno = EINVAL;
        return -1;
    }

    size_t request_len = REQ_CODE_LEN + MAX_SEG_LEN + pathname_length + 1;
    char *request = calloc(request_len, sizeof(char));

    if (!request)
        return -1;

    snprintf(request, request_len, "%d%010ld%s", CLOSE_FILE, pathname_length, pathname);

    if (writen(fd_skt, request, request_len - 1) == -1)
        return -1;

    free(request);

    SERVER_RESPONSE(closeFile, pathname);

    return errno ? -1 : 0;
}