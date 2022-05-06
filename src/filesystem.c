#include "../include/define_source.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/filesystem.h"
#include "../include/utils.h"
#include "../include/mutex.h"

/**
 * @brief Alloca e inizializza un file con pathname @param path pathname del file.
 * 
 * \retval NULL se non è stato possibile allocare o inizializzare il file (errno settato)
 * \retval newFile puntatore al file allocato
 */
static File *initFile(const char *path)
{
    if (!path)
    {
        errno = EINVAL;
        return NULL;
    }

    File *newFile;
    // alloco e inizializzo una struttura di tipo File, ritorno NULL se c'è stato un errore
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFile, perror("calloc"); return NULL, 1, sizeof(File));

    //duplico il path del file, ritorno NULL se c'è stato un errore
    CHECK_RET_AND_ACTION(strndup, ==, NULL, newFile->path, perror("strndup"); return NULL, path, strlen(path) + 1);

    return newFile;
}

static void deleteFile(void* file)
{
    File *tmp = (File *) file;

    if (tmp->path)
        free(tmp->path);

    if (tmp)
        free(tmp);
}

static int addFile(Filesystem *fs, File *file)
{
    if (!fs || !file)
    {
        errno = EINVAL;
        return -1;
    }

    if (!icl_hash_insert(fs->hastTable, file->path, file))
        return -1;

    return 0;
}

/**
 * @brief Alloco e inizializzo un filesystem.
 * 
 * @param maxFiles numero massimo di file che possono essere memorizzati nel filesystem
 * @param maxMemory spazio massimo che può occupare il filesystem (in bytes)
 * 
 * \retval NULL se c'è stato un errore allocando o inizializzando il filesystem (errno settato)
 * \retval newFilesystem puntatore al filesystem allocato
 */
Filesystem *initFileSystem(long maxFiles, long maxMemory)
{
    if (maxFiles <= 0 || maxMemory <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    Filesystem *newFilesystem;
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
                         errno = errnum;
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