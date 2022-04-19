#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define ARRAY_LENGTH(arr, arr_length)                                                                  \
    arr_length = sizeof(*ptrArr);     /* spezzo in due la divisione per eviatre -Wsizeof-pointer-div*/ \
    arr_length /= sizeof(*ptrArr[0]); /*non e'propriamente POSIX conforme, ma per questa volta può andare bene*/

#define SYSCALL_EQ_RETURN(syscall, val) \
    if (syscall == val)                 \
    {                                   \
        perror(#syscall);               \
        return -1;                      \
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