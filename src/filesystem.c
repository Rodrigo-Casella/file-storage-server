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
    CHECK_RET_AND_ACTION(strdup, ==, NULL, newFile->path, perror("strdup"); return NULL, path);
    newFile->path[strlen(path)] = '\0';

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

    // alloco e inizializzo la lista dei fd che attendo di poter acquisire la lock sul file, ritorno NULL in caso di errore
    CHECK_RET_AND_ACTION(initList, ==, NULL, newFile->waitingForLock, perror("initList"); return NULL, NULL);
    // inizialmente il file non è in stato di locked
    newFile->lockedBy = 0;

    return newFile;
}

static void freeFile(void *filePtr)
{
    File *file = (File *)filePtr;

    if (file->waitingForLock)
        deleteList(&(file->waitingForLock));

    if (file->openedBy)
        deleteList(&(file->openedBy));

    if (file->path)
        free(file->path);

    if (file->data)
        free(file->data);

    int errnum = 0;
    CHECK_RET_AND_ACTION(pthread_mutex_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(errnum));
                         errno = errnum,
                         &(file->fileLock));

    CHECK_RET_AND_ACTION(pthread_cond_destroy, !=, 0, errnum,
                         fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(errnum));
                         errno = errnum, &(file->readWrite));

    if (file)
        free(file);
}

static void deleteFile(Filesystem *fs, File *file, fdList *signalForLock)
{
    LOCK(&(file->fileLock));

    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    if (file->waitingForLock && signalForLock)
        signalForLock = file->waitingForLock;

    if (file->openedBy)
        deleteList(&(file->openedBy));

    fs->currFiles--;
    fs->currMemory -= file->dataSize;

    UNLOCK(&(file->fileLock));

    if (icl_hash_delete(fs->hastTable, file->path, NULL, &freeFile) == -1)
        pthread_exit((void *)EXIT_FAILURE);
}

static File *getFile(Filesystem *fs, const char *path)
{
    return (File *)icl_hash_find(fs->hastTable, (void *)path);
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

    icl_hash_destroy(fs->hastTable, NULL, &freeFile);

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

    File *file = getFile(fs, path);

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

    File *file = getFile(fs, path);

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

    File *file = getFile(fs, path);

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

    if (!file->nReaders)
    {
        SIGNAL(&(file->readWrite));
    }

    file->nReaders--;

    UNLOCK(&(file->fileLock));

    return errno ? -1 : 0;
}

int readNFilesHandler(Filesystem *fs, const int upperLimit, void **data_buf, size_t *dataSize)
{
    int readCount;

    size_t bufCurrSize = 0;

    char *file_data_buf = NULL;

    File *currFile;

    icl_hash_iter_t *iter;

    if (!fs)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    iter = icl_hash_iterator_create(fs->hastTable);

    if (!iter)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOMEM;
        return -1;
    }

    if (icl_hash_next(iter) == 0)
        goto cleanup;

    currFile = (File *)iter->currEntry->data;

    readCount = 0;
    while (currFile && (readCount < upperLimit || upperLimit <= 0))
    {
        size_t path_len = strlen(currFile->path) + 1;

        size_t bufNewSize = bufCurrSize + sizeof(size_t) + path_len + sizeof(size_t) + currFile->dataSize;

        void *tmp = realloc(file_data_buf, bufNewSize);
        if (!tmp)
        {
            UNLOCK(&(fs->fileSystemLock));
            errno = ENOMEM;
            free(file_data_buf);
            return -1;
        }

        file_data_buf = tmp;

        memcpy(file_data_buf + bufCurrSize, &path_len, sizeof(size_t)); // Copio la dimensione del path del file

        memcpy(file_data_buf + bufCurrSize + sizeof(size_t), currFile->path, path_len); // Copio il path del file

        memcpy(file_data_buf + bufCurrSize + sizeof(size_t) + path_len, &(currFile->dataSize), sizeof(size_t)); // Copio la dimensione del file

        memcpy((file_data_buf + bufNewSize - (currFile->dataSize)), currFile->data, currFile->dataSize); // Copio il file

        bufCurrSize = bufNewSize;

        readCount++;

        if (icl_hash_next(iter) == 0)
            break;

        currFile = (File *)iter->currEntry->data;
    }

cleanup:
    icl_hash_iterator_destroy(iter);

    UNLOCK(&(fs->fileSystemLock));

    *data_buf = file_data_buf;
    *dataSize = bufCurrSize;

    return readCount;
}

int lockFileHandler(Filesystem *fs, const char *path, int clientFd)
{
    File *file;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock));

    UNLOCK(&(fs->fileSystemLock));

    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    if (file->lockedBy && file->lockedBy != clientFd)
    {
        CHECK_AND_ACTION(insertNode, ==, -1, perror("insertNode"); pthread_exit((void*)EXIT_FAILURE),file->waitingForLock, clientFd);
        UNLOCK(&(fs->fileSystemLock));
        return -2;
    }

    file->lockedBy = clientFd;

    BCAST(&(file->readWrite));

    UNLOCK(&(file->fileLock));

    return 0;
}

int unlockFileHandler(Filesystem *fs, const char *path, int *nextLockFd, int clientFd)
{
    File *file;
    fdNode *nextLock;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock));

    UNLOCK(&(fs->fileSystemLock));

    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    // il file è in stato di lock da parte di un altro processo
    if (file->lockedBy != clientFd)
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EACCES;
        return -1;
    }

    *nextLockFd = file->lockedBy = 0;

    nextLock = popNode(file->waitingForLock);

    if (nextLock)
        *nextLockFd = file->lockedBy = nextLock->fd;

    BCAST(&(file->readWrite));

    UNLOCK(&(file->fileLock));

    return 0;
}

int removeFileHandler(Filesystem *fs, const char *path, fdList *signalForLock, int clientFd)
{
    File *file;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

    // il file non esiste oppure è in stato di lock da parte di un altro processo e/o non è in stato di lock
    if (!file || file->lockedBy != clientFd)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = !file ? ENOENT : EACCES;
        return -1;
    }

    deleteFile(fs, file, signalForLock);

    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

int closeFileHandler(Filesystem *fs, const char *path, int clientFd)
{
    File *file;
    fdNode *fdToClose;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

    // il file che voglio chiudere non esiste
    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock));

    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    // il file non è stato aperto dal client
    if (!(fdToClose = getNode(file->openedBy, clientFd)))
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EPERM;
        return -1;
    }

    deleteNode(fdToClose);

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

int clientExitHandler(Filesystem *fs, fdList *signalForLock, int clientFd)
{
    File *currFile;

    icl_hash_iter_t *iter;

    fdNode *tmp;

    if (!fs || clientFd <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    iter = icl_hash_iterator_create(fs->hastTable);

    if (!iter)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOMEM;
        return -1;
    }

    if (icl_hash_next(iter) == 0)
        goto cleanup;

    currFile = (File *)iter->currEntry->data;

    while (currFile)
    {
        LOCK(&(currFile->fileLock));

        while (currFile->nReaders > 0 || currFile->isWritten)
        {
            WAIT(&(currFile->readWrite), &(currFile->fileLock));
        }

        if (currFile->lockedBy == clientFd)
        {
            fdNode *nextNode = popNode(currFile->waitingForLock);

            currFile->lockedBy = 0;

            if (nextNode)
            {
                insertNode(signalForLock, nextNode->fd);
                currFile->lockedBy = nextNode->fd;
                deleteNode(nextNode);
            }
        }

        tmp = getNode(currFile->waitingForLock, clientFd);

        if (tmp)
            deleteNode(tmp);

        tmp = getNode(currFile->openedBy, clientFd);

        if (tmp)
            deleteNode(tmp);

        UNLOCK(&(currFile->fileLock));

        if (icl_hash_next(iter) == 0)
            break;

        currFile = (File *)iter->currEntry->data;
    }

cleanup:
    icl_hash_iterator_destroy(iter);

    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

int canWrite(Filesystem *fs, const char *path, int clientFd)
{
    int canWrite;
    File *file;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

    // il file non esiste
    if (!file)
    {
        UNLOCK(&(fs->fileSystemLock));
        errno = ENOENT;
        return -1;
    }

    LOCK(&(file->fileLock));

    canWrite = (file->lockedBy == clientFd && findNode(file->openedBy, clientFd));

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return canWrite;
}