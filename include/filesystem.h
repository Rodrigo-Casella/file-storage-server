#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <pthread.h>
#include <time.h>

typedef struct file
{
    char *path;
    void *data;
    long dataSize;

    time_t insertionTime;

    pthread_rwlock_t rwlock;

    struct file *next;
} File;

typedef struct filesystem
{
    long maxFiles;
    long maxMemory;

    File *fileList;

    pthread_mutex_t fileSystemLock;
} Filesystem;

Filesystem *initFileSystem(long maxFiles, long maxMemory);
void deleteFileSystem(Filesystem **fs);
void printFileSystem(Filesystem *fs);
void addDummyFiles(Filesystem *fs);
#endif