#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <pthread.h>
#include <time.h>

#include "../include/icl_hash.h"

typedef struct fd_node
{
    struct fd_node *prev;
    int fd;
    struct fd_node *next;
} fdNode;

typedef struct fd_list
{
    fdNode *head;
    fdNode *tail;
} fdList;

typedef struct file
{
    char *path;
    char *data;
    size_t dataSize;

    pthread_mutex_t fileLock;
    pthread_cond_t readWrite;

    short isWritten;
    int nReaders;

    fdList *openedBy;
    int lockedBy;
} File;

typedef struct filesystem
{
    long maxFiles;
    long currFiles;
    long absMaxFiles;

    size_t maxMemory;
    size_t currMemory;
    size_t absMaxMemory;

    icl_hash_t *hastTable;

    pthread_mutex_t fileSystemLock;
} Filesystem;

Filesystem *initFileSystem(long maxFiles, long maxMemory);
void deleteFileSystem(Filesystem *fs);
void printFileSystem(Filesystem *fs);
int openFileHandler(Filesystem *fs, const char* path, int openFlags, int clientFd);
int closeFileHandler(Filesystem *fs, const char* path, int clientFd);
int writeFileHandler(Filesystem *fs, const char* path, void* data, size_t dataSize, int clientFd);
void addDummyFiles(Filesystem *fs);
#endif