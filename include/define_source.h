#ifndef DEFINE_SOURCE_H
#define DEFINE_SOURCE_H

#if !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
    #define _POSIX_C_SOURCE 200809L
#endif

#endif