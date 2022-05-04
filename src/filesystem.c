#include "../include/define_source.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/filesystem.h"
#include "../include/utils.h"
#include "../include/mutex.h"

static File *initFile(const char *path)
{
    File *newFile = NULL;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFile, perror("calloc"); return NULL, 1, sizeof(File));

    newFile->path = NULL;
    CHECK_RET_AND_ACTION(strndup, ==, NULL, newFile->path, perror("strndup"); return NULL, path, strlen(path) + 1);

    newFile->data = NULL;
    newFile->dataSize = 0;
    newFile->insertionTime = time(NULL);

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_rwlock_init, !=, 0, errnum,
                         fprintf(stderr, "pthread_rwlock_init: %s", strerror(errnum));
                         return NULL, &(newFile->rwlock), NULL);

    return newFile;
}

static void deleteFile(File *file)
{
    if (file->path)
        free(file->path);
    if (file->data)
        free(file->data);

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_rwlock_destroy, !=, 0, errnum, fprintf(stderr, "pthread_rwlock_destroy: %s", strerror(errnum)), &(file->rwlock));
    free(file);
}

static void addFileToList(File **head, File *file)
{
    file->next = *head;
    *head = file;
}

static void deleteFileList(File **head)
{
    File *tmp;

    while (*head)
    {
        tmp = *head;
        *head = (*head)->next;

        deleteFile(tmp);
    }
}

Filesystem *initFileSystem(long maxFiles, long maxMemory)
{
    Filesystem *newFilesystem = NULL;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFilesystem, perror("calloc"); return NULL, 1, sizeof(Filesystem));

    newFilesystem->maxMemory = maxMemory;
    newFilesystem->maxFiles = maxFiles;
    newFilesystem->fileList = NULL;

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_init, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_init: %s\n", strerror(errnum));
                         return NULL,
                                &(newFilesystem->fileSystemLock), NULL);
    return newFilesystem;
}

void deleteFileSystem(Filesystem **fs)
{
    if (!*fs)
    {
        errno = EINVAL;
        return;
    }

    deleteFileList(&((*fs)->fileList));

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_destroy: %s", strerror(errnum)), &((*fs)->fileSystemLock));

    free(*fs);
}

void printFileSystem(Filesystem *fs)
{
    File *tmp = fs->fileList;

    while (tmp)
    {
        printf("Filename: %s\n", tmp->path);
        tmp = tmp->next;
    }
}

void addDummyFiles(Filesystem *fs)
{
    char buf[6];
    
    for (size_t i = 0; i < 10; i++)
    {
        snprintf(buf, 6, "file%ld", i);
        File *newFile = initFile(buf);

        LOCK(&(fs->fileSystemLock));

        addFileToList(&(fs->fileList), newFile);
        
        UNLOCK(&(fs->fileSystemLock));
    }
}