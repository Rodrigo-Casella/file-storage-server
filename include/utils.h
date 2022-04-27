#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#define UNIX_PATH_MAX 108

#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define LOCK(l)                                  \
    if (pthread_mutex_lock(l) != 0)              \
    {                                            \
        fprintf(stderr, "ERRORE FATALE lock\n"); \
        pthread_exit((void *)EXIT_FAILURE);      \
    }
#define UNLOCK(l)                                  \
    if (pthread_mutex_unlock(l) != 0)              \
    {                                              \
        fprintf(stderr, "ERRORE FATALE unlock\n"); \
        pthread_exit((void *)EXIT_FAILURE);        \
    }
#define WAIT(c, l)                               \
    if (pthread_cond_wait(c, l) != 0)            \
    {                                            \
        fprintf(stderr, "ERRORE FATALE wait\n"); \
        pthread_exit((void *)EXIT_FAILURE);      \
    }

#define TWAIT(c, l, t)                                                    \
    {                                                                     \
        int r = 0;                                                        \
        if ((r = pthread_cond_timedwait(c, l, t)) != 0 && r != ETIMEDOUT) \
        {                                                                 \
            fprintf(stderr, "ERRORE FATALE timed wait\n");                \
            pthread_exit((void *)EXIT_FAILURE);                           \
        }                                                                 \
    }
#define SIGNAL(c)                                  \
    if (pthread_cond_signal(c) != 0)               \
    {                                              \
        fprintf(stderr, "ERRORE FATALE signal\n"); \
        pthread_exit((void *)EXIT_FAILURE);        \
    }
#define BCAST(c)                                      \
    if (pthread_cond_broadcast(c) != 0)               \
    {                                                 \
        fprintf(stderr, "ERRORE FATALE broadcast\n"); \
        pthread_exit((void *)EXIT_FAILURE);           \
    }
static inline int TRYLOCK(pthread_mutex_t *l)
{
    int r = 0;
    if ((r = pthread_mutex_trylock(l)) != 0 && r != EBUSY)
    {
        fprintf(stderr, "ERRORE FATALE unlock\n");
        pthread_exit((void *)EXIT_FAILURE);
    }
    return r;
}

#define SYSCALL_EQ_RETURN(syscall, val, ...) \
    if (syscall(__VA_ARGS__) == val)         \
    {                                        \
        perror(#syscall);                    \
        return -1;                           \
    }

#define SYSCALL_EQ_ACTION(syscall, val, action, ...) \
    if (syscall(__VA_ARGS__) == val)                 \
    {                                                \
        perror(#syscall);                            \
        action;                                      \
    }

#define SYSCALL_RET_EQ_ACTION(syscall, val, retval, action, ...) \
    if ((retval = syscall(__VA_ARGS__)) == val)                  \
    {                                                            \
        perror(#syscall);                                        \
        action;                                                  \
    }

#define SYSCALL_NEQ_RETURN(syscall, val, ...) \
    if (syscall(__VA_ARGS__) != val)          \
    {                                         \
        perror(#syscall);                     \
        return -1;                            \
    }

#define SYSCALL_NEQ_ACTION(syscall, val, action, ...) \
    if (syscall(__VA_ARGS__) != val)                  \
    {                                                 \
        perror(#syscall);                             \
        action;                                       \
    }

#define SYSCALL_RET_NEQ_ACTION(syscall, val, retval, action, ...) \
    if ((retval = syscall(__VA_ARGS__)) != val)                   \
    {                                                             \
        perror(#syscall);                                         \
        action;                                                   \
    }

#define SYSCALL_OP_ACTION(syscall, op, val, action, ...) \
    if (syscall(__VA_ARGS__) op val)                     \
    {                                                    \
        perror(#syscall);                                \
        action;                                          \
    }

#define CHECK_AND_ACTION(func, op, val, action, ...) \
    if (func(__VA_ARGS__) op val)                    \
    {                                                \
        action;                                      \
    }

#define CHECK_RET_AND_ACTION(func, op, val, retval, action, ...) \
    if ((retval = func(__VA_ARGS__)) op val)                     \
    {                                                            \
        action;                                                  \
    }

#define FOR_ACTION(condition, action) \
    for (condition)                   \
    {                                 \
        action;                       \
    }

#define TOKENIZER(string, del, action)                                \
    if (1)                                                            \
    {                                                                 \
        char *save_ptr;                                               \
        char *token = strtok_r(string, del, &save_ptr);               \
        for (; token; token = strtok_r(NULL, del, &save_ptr), action) \
        {                                                             \
            action;                                                   \
        }                                                             \
    }

#define TIME_IS_ZERO(time) (time.tv_sec == 0 && time.tv_nsec == 0)
static inline void dupNTokens(char *string, const char *del, int n, ...)
{
    va_list ap;
    va_start(ap, n);
    char *token, *savePtr;
    token = strtok_r(string, del, &savePtr);
    for (int i = 0; token && i < n; i++)
    {
        char **string = va_arg(ap, char **);
        *string = strndup(token, strlen(token) + 1);
        token = strtok_r(NULL, del, &savePtr);
    }
    va_end(ap);
}

static inline void freeNargs(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++)
    {
        void **arg = va_arg(ap, void **);
        if (*arg)
            free(*arg);
    }
}

static inline void setTimespecMsec(struct timespec *time, long msec)
{
    int sec = msec / 1000;
    long nanosec = ((msec % 1000) * 1000000);
    if (nanosec > 999999999)
    {
        fprintf(stderr, "Tempo intervallo tra richieste in nanosecondi fuori range.\n");
        sec = 0, nanosec = 0;
    }
    (*time).tv_sec = sec;
    (*time).tv_nsec = nanosec;
}

static inline int isNumber(const char *s, long *n)
{
    if (s == NULL)
        return 1;
    if (strlen(s) == 0)
        return 1;
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);
    if (errno == ERANGE)
        return 2;
    if (e != NULL && *e == (char)0)
    {
        *n = val;
        return 0;
    }
    return 1;
}

static inline int readn(long fd, void *buf, size_t size)
{
    size_t left = size;
    int r;
    char *bufptr = (char *)buf;
    while (left > 0)
    {
        if ((r = read((int)fd, bufptr, left)) == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        left -= r;
        bufptr += r;
    }
    return size;
}

static inline int writen(long fd, void *buf, size_t size)
{
    size_t left = size;
    int r;
    char *bufptr = (char *)buf;
    while (left > 0)
    {
        if ((r = write((int)fd, bufptr, left)) == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return 0;
        left -= r;
        bufptr += r;
    }
    return 1;
}

#endif