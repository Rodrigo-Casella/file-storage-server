#ifndef DEFINE_SOURCE_H
#define DEFINE_SOURCE_H

#if !defined(_XOPEN_SOURCE)
    #define _XOPEN_SOURCE 700
#elif _XOPEN_SOURCE < 700
    #define _XOPEN_SOURCE 700
#endif

#endif