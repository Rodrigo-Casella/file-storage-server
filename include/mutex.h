#ifndef MUTEX_H
#define MUTEX_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define WRITE_LOCK(l)                              \
    if (pthread_rwlock_wrlock(l) != 0)             \
    {                                              \
        fprintf(stderr, "ERRORE FATALE wrlock\n"); \
        pthread_exit((void *)EXIT_FAILURE);        \
    }
#define READ_LOCK(l)                               \
    if (pthread_rwlock_rdlock(l) != 0)             \
    {                                              \
        fprintf(stderr, "ERRORE FATALE rdlock\n"); \
        pthread_exit((void *)EXIT_FAILURE);        \
    }
#define RW_UNLOCK(l)                                      \
    if (pthread_rwlock_unlock(l) != 0)                    \
    {                                                     \
        fprintf(stderr, "ERRORE FATALE rwlock_unlock\n"); \
        pthread_exit((void *)EXIT_FAILURE);               \
    }
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
#endif