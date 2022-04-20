#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define ARRAY_LENGTH(arr, arr_length)                                                                  \
    arr_length = sizeof(*ptrArr);     /* spezzo in due la divisione per eviatre -Wsizeof-pointer-div*/ \
    arr_length /= sizeof(*ptrArr[0]); /*non e'propriamente POSIX conforme, ma per questa volta può andare bene*/

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
        fprintf(stderr, "Errore in %s.\n", #func);   \
        action;                                      \
    }

#define CHECK_RET_AND_ACTION(func, op, val, retval, action, ...) \
    if ((retval = func(__VA_ARGS__)) op val)                     \
    {                                                            \
        fprintf(stderr, "Errore in %s.\n", #func);               \
        action;                                                  \
    }

#define FOR_ACTION(condition, action) \
    for (condition)                   \
    {                                 \
        action;                       \
    }

#define TOKENIZER(string, del, action)                                                                                     \
    if (1)                                                                                                                 \
    {                                                                                                                      \
        char *save_ptr;                                                                                                    \
        FOR_ACTION(char *token = strtok_r(string, del, &save_ptr); token; token = strtok_r(NULL, del, &save_ptr), action); \
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

#endif