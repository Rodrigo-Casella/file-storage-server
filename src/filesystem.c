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
    File *newFile;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFile, perror("calloc"); return NULL, 1, sizeof(File));

    CHECK_RET_AND_ACTION(strndup, ==, NULL, newFile->path, perror("strndup"); return NULL, path, strlen(path) + 1);

    return newFile;
}

static void deleteFile(void* file)
{
    File *tmp = (File *) file;

    if (tmp->path)
        free(tmp->path);

    free(tmp);
}

static void addFile(Filesystem *fs, File *file)
{
    icl_hash_insert(fs->hastTable, file->path, file);
}

Filesystem *initFileSystem(long maxFiles, long maxMemory)
{
    Filesystem *newFilesystem = NULL;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFilesystem, perror("calloc"); return NULL, 1, sizeof(Filesystem));

    newFilesystem->maxMemory = maxFiles;
    newFilesystem->currFiles = 0;

    newFilesystem->maxFiles = maxMemory;
    newFilesystem->currMemory = 0;

    newFilesystem->hastTable = icl_hash_create((int) (maxFiles * (0.75F)), NULL, NULL);
    if (!newFilesystem->hastTable)
    {
        fprintf(stderr, "Errore creazione hash table per il filesystem\n");
        return NULL;
    }

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_init, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_init: %s\n", strerror(errnum));
                         return NULL,
                                &(newFilesystem->fileSystemLock), NULL);
    return newFilesystem;
}

void deleteFileSystem(Filesystem *fs)
{
    if (!fs)
    {
        errno = EINVAL;
        return;
    }

    icl_hash_destroy(fs->hastTable, NULL, &deleteFile);

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_destroy: %s", strerror(errnum)), &(fs->fileSystemLock));

    free(fs);
}

void printFileSystem(Filesystem *fs)
{
    char *fileName;
    File *file;
    int i;
    icl_entry_t *entry;
    icl_hash_foreach(fs->hastTable, i, entry, fileName, file, printf("File: %s\n", fileName));
}

void addDummyFiles(Filesystem *fs)
{
    char buf[10];
    
    LOCK(&(fs->fileSystemLock));

    for (size_t i = 0; i < 10; i++)
    {
        
        snprintf(buf, 10, "file%ld", fs->currFiles++);
        File *newFile = initFile(buf);

        printf("Thread: %ld adding file: %s\n", pthread_self(), newFile->path);
        addFile(fs, newFile);
    }

    UNLOCK(&(fs->fileSystemLock));
}