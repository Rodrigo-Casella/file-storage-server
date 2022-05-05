#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <pthread.h>
#include <time.h>

#include "../include/icl_hash.h"

typedef struct file
{
    char *path;
} File;

typedef struct filesystem
{
    long maxFiles;
    long currFiles;

    long maxMemory;
    long currMemory;

    icl_hash_t *hastTable;

    pthread_mutex_t fileSystemLock;
} Filesystem;

Filesystem *initFileSystem(long maxFiles, long maxMemory);
void deleteFileSystem(Filesystem *fs);
void printFileSystem(Filesystem *fs);
void addDummyFiles(Filesystem *fs);
#endif