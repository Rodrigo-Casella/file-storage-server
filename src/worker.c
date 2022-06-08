#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>

#include "../include/worker.h"
#include "../include/utils.h"
#include "../include/message_protocol.h"

#define THREAD_ERR_EXIT                                         \
    fprintf(stderr, "Errore in thread: %ld\n", pthread_self()); \
    pthread_exit((void *)EXIT_FAILURE);

#define SEND_RESPONSE_CODE(fd, response_code)                           \
    snprintf(response_code_buf, RES_CODE_LEN + 1, "%d", response_code); \
    if (writen(*client_fd, response_code_buf, RES_CODE_LEN) == -1)      \
    {                                                                   \
        THREAD_ERR_EXIT;                                                \
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

static char *readSegment(int fd, long *data_size)
{
    char segment_len_buf[MAX_SEG_LEN + 1] = "";
    long segment_len = 0;
    char *segment = NULL;

    if (readn(fd, segment_len_buf, MAX_SEG_LEN) == -1)
    {
        return NULL;
    }

    if (isNumber(segment_len_buf, &segment_len) != 0)
    {
        return NULL;
    }

    if (data_size)
        *data_size = segment_len;

    segment = calloc(segment_len + 1, sizeof(char));

    if (!segment)
    {
        return NULL;
    }

    if (readn(fd, segment, segment_len) == -1)
    {
        return NULL;
    }
    return segment;
}

void *processRequest(void *args)
{
    BQueue_t *client_request_queue = ((ThreadArgs *)args)->queue;
    Filesystem *fs = ((ThreadArgs *)args)->fs;
    int managerFd = ((ThreadArgs *)args)->write_end_pipe_fd;

    while (1)
    {
        int *client_fd = pop(client_request_queue);

        if (client_fd == EOS)
            break;

        char request_code_buf[REQ_CODE_LEN + 1] = "",
                                             pipe_buf[PIPE_BUF_LEN + 1] = "",
                                             open_file_flag_buf[OPEN_FLAG_LEN + 1] = "",
                                             response_code_buf[RES_CODE_LEN + 1] = "";

        char *request_buf,
            *file_data;

        long request_code = 0,
             open_file_flag = 0,
             file_size = 0;

        if (read((*client_fd), request_code_buf, REQ_CODE_LEN) == 0)
        {

            SYSCALL_EQ_ACTION(close, -1, THREAD_ERR_EXIT, (*client_fd));
            free(client_fd);
            CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, CLIENT_LEFT_MSG, PIPE_BUF_LEN);

            continue;
        }

        if (isNumber(request_code_buf, &request_code) != 0)
        {
            SEND_RESPONSE_CODE(*client_fd, INVALID_REQ);
        }

        request_buf = readSegment(*client_fd, NULL);

        if (!request_buf)
        {
            SEND_RESPONSE_CODE(*client_fd, SERVER_ERR);
        }

        switch (request_code)
        {
        case OPEN_FILE:
            if (readn(*client_fd, open_file_flag_buf, OPEN_FLAG_LEN) == -1)
            {
                // TODO: handle error
            }

            if (isNumber(open_file_flag_buf, &open_file_flag) != 0)
            {
                // TODO: handle error
            }

            if (openFileHandler(fs, request_buf, (int)open_file_flag, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        case WRITE_FILE:
            file_data = readSegment(*client_fd, &file_size);

            if (!file_data)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            if (writeFileHandler(fs, request_buf, file_data, file_size, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            free(file_data);

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        case CLOSE_FILE:
            if (closeFileHandler(fs, request_buf, *client_fd) == -1)
            {
                SEND_ERROR_CODE(*client_fd)
                break;
            }

            SEND_RESPONSE_CODE(*client_fd, SUCCESS);
            break;
        default:
            break;
        }

        free(request_buf);

        snprintf(pipe_buf, PIPE_BUF_LEN, "%d", *client_fd);
        CHECK_AND_ACTION(writen, ==, -1, perror("writen"); THREAD_ERR_EXIT, managerFd, pipe_buf, PIPE_BUF_LEN);
        free(client_fd);
    }

    pthread_exit(NULL);
}