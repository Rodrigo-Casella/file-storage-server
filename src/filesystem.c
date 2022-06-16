#include "../include/define_source.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"
#include "../include/filesystem.h"
#include "../include/mutex.h"
#include "../include/utils.h"

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

    // duplico il path del file, ritorno NULL se c'è stato un errore
    CHECK_RET_AND_ACTION(strndup, ==, NULL, newFile->path, perror("strndup"); return NULL, path, strlen(path) + 1);

    newFile->data = NULL;
    newFile->dataSize = 0;

    // inizializzo la mutex del file, setto errno e ritorno NULL in caso di errore
    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_init, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_init: %s\n", strerror(errnum));
                         errno = errnum;
                         return NULL,
                                &(newFile->fileLock), NULL);

    // iniziallizzo la variabile di condizione
    CHECK_RET_AND_ACTION(pthread_cond_init, !=, 0, errnum,
                         fprintf(stderr, "pthread_cond_init: %s\n", strerror(errnum));
                         errno = errnum;
                         return NULL,
                                &(newFile->readWrite), NULL);

    newFile->isWritten = 0;
    newFile->nReaders = 0;
    // alloco e inizializzo la lista dei fd che hanno aperto il file, ritorno NULL in caso di errore
    CHECK_RET_AND_ACTION(initList, ==, NULL, newFile->openedBy, perror("initList"); return NULL, NULL);
    // inizialmente il file non è in stato di locked
    newFile->lockedBy = 0;

    return newFile;
}

static void deleteFile(void *file)
{
    File *tmp = (File *)file;

    if (tmp->path)
        free(tmp->path);

    if (tmp->data)
        free(tmp->data);

    if (tmp->openedBy)
        deleteList(tmp->openedBy);

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(errnum));
                         errno = errnum,
                         &(tmp->fileLock));

    CHECK_RET_AND_ACTION(pthread_cond_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(errnum));
                         errno = errnum, &(tmp->readWrite));

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

    fs->currFiles++;
    fs->absMaxFiles = MAX(fs->absMaxFiles, fs->currFiles);

    fs->currMemory += file->dataSize;
    fs->absMaxMemory = MAX(fs->absMaxMemory, fs->currMemory);

    return 0;
}

Filesystem *initFileSystem(long maxFiles, long maxMemory)
{
    if (maxFiles <= 0 || maxMemory <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    Filesystem *newFilesystem;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newFilesystem, perror("calloc"); return NULL, 1, sizeof(Filesystem));

    newFilesystem->maxFiles = maxFiles;
    newFilesystem->currFiles = 0;
    newFilesystem->absMaxFiles = 0;

    newFilesystem->maxMemory = maxMemory;
    newFilesystem->currMemory = 0;
    newFilesystem->absMaxMemory = 0;

    newFilesystem->hastTable = icl_hash_create((int)(maxFiles * (0.75F)), NULL, NULL);
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
    char *tmpFileName;
    File *tmpFile;
    int tmpIndex;
    icl_entry_t *tmpEntry;
    icl_hash_foreach(fs->hastTable, tmpIndex, tmpEntry, tmpFileName, tmpFile, printf("File: %s, dataSize: %ld\n", tmpFileName, tmpFile->dataSize));
}

int openFileHandler(Filesystem *fs, const char *path, int openFlags, int clientFd)
{
    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    int create = FLAG_ISSET(openFlags, O_CREATE);
    int lock = FLAG_ISSET(openFlags, O_LOCK);

    LOCK(&(fs->fileSystemLock));

    File *file = (File *)icl_hash_find(fs->hastTable, (void *)path);

    // il file non esiste, ma non ho settato la flag O_CREATE
    if (!file && !create)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    // il file esiste, ma ho settato la flag O_CREATE
    if (file && create)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = EEXIST;
        return -1;
    }

    // Dopo i check precedenti qua sono sicuro di creare un nuovo file solo se non esiste
    if (create)
    {
        file = initFile(path);

        if (!file)
        {
            UNLOCK(&(fs->fileSystemLock));
            return -1;
        }

        if (fs->currFiles == fs->maxFiles)
        {
            // TODO: Expell file from storage
        }

        addFile(fs, file);
    }

    LOCK(&(file->fileLock));

    if (lock)
    {
        // il file è già in stato di lock
        if (file->lockedBy && (file->lockedBy != clientFd))
        {
            UNLOCK(&(file->fileLock));
            UNLOCK(&(fs->fileSystemLock));
            errno = EACCES;
            return -1;
        }

        file->lockedBy = clientFd;
    }

    // Se il file non è stato già aperto dal client allora aggiungo il suo fd alla lista dei processi che hanno aperto il file
    if (!findNode(file->openedBy, clientFd))
    {
        CHECK_AND_ACTION(insertNode, ==, -1,
                         UNLOCK(&(file->fileLock));
                         UNLOCK(&(fs->fileSystemLock)); errno = ENOMEM; return -1, file->openedBy, clientFd);
    }

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

int writeFileHandler(Filesystem *fs, const char *path, void *data, size_t dataSize, int clientFd)
{
    if (!fs || !path || !data || (dataSize <= 0) || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    File *file = (File *)icl_hash_find(fs->hastTable, (void *)path);

    // il file che voglio scrivere non esiste
    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock))

    // il file è in stato di lock oppure il client non ha aperto il file
    if ((file->lockedBy && file->lockedBy != clientFd) || !findNode(file->openedBy, clientFd))
    {
        UNLOCK(&(file->fileLock));
        errno = EACCES;
        return -1;
    }

    // I dati da scrivere sono troppi per lo storage
    if ((file->dataSize + dataSize) > fs->maxMemory)
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EFBIG;
        return -1;
    }

    // attendo finché non ci sono lettori  o scrittori per scrivere il file
    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    file->isWritten = 1;

    UNLOCK(&(file->fileLock));

    char *fileData;

    while (fs->currMemory + dataSize > fs->maxMemory)
    {
        // TODO: Expell file from storage
        break;
    }

    fileData = calloc((file->dataSize + dataSize), sizeof(*fileData));

    if (!fileData)
    {
        errno = ENOMEM;
        goto cleanup;
    }

    // se il file ha già dei dati copio questi ultimi prima di copiare i dati nuovi
    if (file->data)
    {
        memcpy(fileData, file->data, file->dataSize);
        free(file->data);
    }

    // copio i file in append
    memcpy(fileData + file->dataSize, data, dataSize);
    file->data = fileData;
    file->dataSize += dataSize;

    cleanup:
    LOCK(&(file->fileLock));

    file->isWritten = 0;

    BCAST(&(file->readWrite));

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));
    return errno ? -1 : 0;
}

int readFileHandler(Filesystem *fs, const char *path, void **data_buf, size_t *dataSize, int clientFd)
{
    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    File *file = (File *)icl_hash_find(fs->hastTable, (void *)path);

    // il file che voglio leggere non esiste
    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock))

    UNLOCK(&(fs->fileSystemLock));

    // il file è in stato di lock oppure il client non ha aperto il file
    if ((file->lockedBy && file->lockedBy != clientFd) || !findNode(file->openedBy, clientFd))
    {
        UNLOCK(&(file->fileLock));
        errno = EACCES;
        return -1;
    }

    // attendo finché non ci sono scrittori
    while (file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    file->nReaders++;

    UNLOCK(&(file->fileLock));

    *data_buf = calloc(file->dataSize, sizeof(char));

    if (!(*data_buf))
    {
        errno = ENOMEM;
        goto cleanup;
    }

    memcpy(*data_buf, file->data, file->dataSize);

    *dataSize = file->dataSize;

    cleanup:

    LOCK(&(file->fileLock))

    if(!file->nReaders)
    {
        SIGNAL(&(file->readWrite));
    }

    file->nReaders--;

    UNLOCK(&(file->fileLock));

    return errno ? -1 : 0;
}

int closeFileHandler(Filesystem *fs, const char *path, int clientFd)
{
    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    File *file = (File *)icl_hash_find(fs->hastTable, (void *)path);

    // il file che voglio chiudere non esiste
    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock));

    fdNode *fdToClose;

    // il file non è stato aperto dal client
    if (!(fdToClose = getNode(file->openedBy, clientFd)))
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EPERM;
        return -1;
    }

    deleteNode(fdToClose);

    // se avevo messo il file in stato di lock rilascio la lock al chiuderlo
    if (file->lockedBy && (file->lockedBy == clientFd))
        file->lockedBy = 0;

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return 0;
}