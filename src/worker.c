#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>

#include "../include/fdList.h"
#include "../include/message_protocol.h"
#include "../include/utils.h"
#include "../include/worker.h"

#define THREAD_ERR_EXIT                                         \
    fprintf(stderr, "Errore in thread: %ld\n", pthread_self()); \
    pthread_exit((void *)EXIT_FAILURE);

#define SEND_RESPONSE_CODE(fd, code)                       \
    if (1)                                                 \
    {                                                      \
        int response_code = code;                          \
        if (writen(fd, &response_code, sizeof(int)) == -1) \
        {                                                  \
            perror("writen");                              \
            THREAD_ERR_EXIT;                               \
        }                                                  \
    }

#define SEND_ERROR_CODE(fd)                  \
    switch (errno)                           \
    {                                        \
    case EINVAL:                             \
        SEND_RESPONSE_CODE(fd, INVALID_REQ); \
        break;                               \
    case ENOENT:                             \
        SEND_RESPONSE_CODE(fd, FILENOENT);   \
        break;                               \
    case EEXIST:                             \
        SEND_RESPONSE_CODE(fd, FILEEX);      \
        break;                               \
    case ENOMEM:                             \
        SEND_RESPONSE_CODE(fd, SERVER_ERR);  \
        break;                               \
    case EACCES:                             \
        SEND_RESPONSE_CODE(fd, FILE_LOCK);   \
        break;                               \
    case EFBIG:                              \
        SEND_RESPONSE_CODE(fd, BIG_FILE);    \
        break;                               \
    default:                                 \
        break;                               \
    }

#define SIGNAL_WAITING_FOR_LOCK(signalForLock, responseCode, managerFd)                                                          \
    while (signalForLock->head)                                                                                                  \
    {                                                                                                                            \
        SEND_RESPONSE_CODE(signalForLock->head->fd, responseCode);                                                               \
        CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, &(signalForLock->head->fd), sizeof(int)); \
        fdNode *tmp = signalForLock->head;                                                                                       \
        signalForLock->head = signalForLock->head->next;                                                                         \
        if (tmp->next)                                                                                                           \
            tmp->next->prev = tmp->prev;                                                                                         \
        deleteNode(tmp);                                                                                                         \
    }

static ssize_t readRequestHeader(int fd, int *request_code, size_t *request_len)
{
    struct iovec request_hdr[2];

    memset(request_hdr, 0, sizeof(request_hdr));

    request_hdr[0].iov_base = request_code;
    request_hdr[0].iov_len = sizeof(int);

    request_hdr[1].iov_base = request_len;
    request_hdr[1].iov_len = sizeof(size_t);

    return readv(fd, request_hdr, ARRAY_SIZE(request_hdr));
}

static char *readRequestPayload(int fd, size_t request_len)
{
    char *request_buf;

    request_buf = calloc(request_len, sizeof(char));

    if (!request_buf || readn(fd, request_buf, request_len) == -1)
        return NULL;

    return request_buf;
}

static char *readSegment(int fd, size_t *data_size)
{
    char *segment_buf;

    size_t segment_len;

    if (readn(fd, &segment_len, sizeof(size_t)) == -1)
        return NULL;

    if (data_size)
        *data_size = segment_len;

    segment_buf = calloc(segment_len, sizeof(char));

    if (!segment_buf)
        return NULL;

    if (readn(fd, segment_buf, segment_len) == -1)
        return NULL;

    return segment_buf;
}

static int writeSegment(int fd, char **data_buf, size_t *data_size)
{
    struct iovec segment[2];

    memset(segment, 0, sizeof(segment));

    segment[0].iov_base = data_size;
    segment[0].iov_len = sizeof(size_t);

    segment[1].iov_base = *data_buf;
    segment[1].iov_len = *data_size;

    return writev(fd, segment, ARRAY_SIZE(segment));
}

void *processRequest(void *args)
{
    BQueue_t *client_request_queue = ((ThreadArgs *)args)->queue;
    Filesystem *fs = ((ThreadArgs *)args)->fs;
    int managerFd = ((ThreadArgs *)args)->write_end_pipe_fd;
    fdList *signalForLock = initList();

    if (!signalForLock)
    {
        THREAD_ERR_EXIT;
    }

    while (1)
    {
        int *client_fd = pop(client_request_queue);

        if (client_fd == EOS)
            break;

        char *request_payload = NULL,
             *file_data_buf = NULL;

        int request_code = 0,
            open_file_flag = 0,
            waitForLock = 0;

        long upperLimit = 0;

        size_t file_size = 0,
               request_len = 0;

        if (readRequestHeader(*client_fd, &request_code, &request_len) == -1)
        {
            perror("readRequestHeader");
            SEND_RESPONSE_CODE(*client_fd, SERVER_ERR);
            continue;
        }

        if (!request_code) // Se request_code == 0 vuol dire che il client ha terminato di inviare richieste
        {
            CHECK_AND_ACTION(clientExitHandler, ==, -1, perror("clientExitHandler"); THREAD_ERR_EXIT, fs, signalForLock, *client_fd);
            SYSCALL_EQ_ACTION(close, -1, THREAD_ERR_EXIT, (*client_fd));

            SIGNAL_WAITING_FOR_LOCK(signalForLock, SUCCESS, managerFd);

            *client_fd = 0;
            CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, client_fd, sizeof(int));
            free(client_fd);
            continue;
        }

        request_payload = readRequestPayload(*client_fd, request_len);

        if (!request_payload)
        {
            perror("readRequestPayload");
            SEND_RESPONSE_CODE(*client_fd, SERVER_ERR);
            continue;
        }

        switch (request_code)
        {
        case OPEN_FILE:
            if (readn(*client_fd, &open_file_flag, sizeof(int)) == -1)
            {
                SEND_RESPONSE_CODE(*client_fd, SERVER_ERR);
                break;
            }

            if (openFileHandler(fs, request_payload, open_file_flag, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        case WRITE_FILE:
            if (canWrite(fs, request_payload, *client_fd) == 0)
            {
                SEND_RESPONSE_CODE(*client_fd, INVALID_REQ);
                break;
            }
        case APPEND_FILE:

            file_data_buf = readSegment(*client_fd, &file_size);

            if (!file_data_buf)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            if (writeFileHandler(fs, request_payload, file_data_buf, file_size, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        case READ_FILE:
            if (readFileHandler(fs, request_payload, (void **)&file_data_buf, &file_size, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd);
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);

            if (writeSegment(*client_fd, &file_data_buf, &file_size) == -1)
                fprintf(stderr, "Errore inviando file al client\n");

            break;

        case READ_N_FILE:
            if (isNumber(request_payload, &upperLimit) != 0)
            {
                SEND_RESPONSE_CODE(*client_fd, INVALID_REQ);
                break;
            }

            if (readNFilesHandler(fs, upperLimit, (void **)&file_data_buf, &file_size) == -1)
            {
                SEND_ERROR_CODE(*client_fd);
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);

            if (file_data_buf)
            {
                if (writen(*client_fd, file_data_buf, file_size) == -1)
                    fprintf(stderr, "Errore inviando file al client\n");
            }

            file_size = 0; // Avverto il client che non ci sono più file da leggere
            if (writen(*client_fd, &file_size, sizeof(size_t)) == -1)
                fprintf(stderr, "Errore inviando mesaggio di terminazione al client\n");
            break;
        case LOCK_FILE:;
            int result;

            if ((result = lockFileHandler(fs, request_payload, *client_fd)) == -1)
            {
                SEND_ERROR_CODE(*client_fd);
                break;
            }

            if (result == -2) // Il client deve attendere per acquisire la lock
            {
                waitForLock = 1; // Il suo fd non verra' inserito nel working_set della select
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        case UNLOCK_FILE:;
            int nextLockFd = 0;
            if (unlockFileHandler(fs, request_payload, &nextLockFd, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd);
                break;
            }
            SEND_RESPONSE_CODE(*client_fd, SUCCESS);

            if (nextLockFd)
            {
                SEND_RESPONSE_CODE(nextLockFd, SUCCESS);
                CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, &nextLockFd, sizeof(int));
            }
            break;
        case REMOVE_FILE:
            if (removeFileHandler(fs, request_payload, signalForLock, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd);
                break;
            }
            SEND_RESPONSE_CODE(*client_fd, SUCCESS);

            SIGNAL_WAITING_FOR_LOCK(signalForLock, FILENOENT, managerFd);
            break;
        case CLOSE_FILE:
            if (closeFileHandler(fs, request_payload, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        default:
            SEND_RESPONSE_CODE(*client_fd, INVALID_REQ);
            break;
        }

        if (request_payload)
            free(request_payload);

        if (file_data_buf)
            free(file_data_buf);

        if (!waitForLock)
        {
            CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, client_fd, sizeof(int));
            free(client_fd);
        }
    }

    deleteList(&signalForLock);

    pthread_exit(NULL);
}