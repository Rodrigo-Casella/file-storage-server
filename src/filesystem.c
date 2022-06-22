#include "../include/define_source.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/compare_func.h"
#include "../include/definitions.h"
#include "../include/filesystem.h"
#include "../include/mutex.h"
#include "../include/utils.h"

#define UPDATE_FILE_REFERENCE(file) \
    file->lastUsed = time(NULL);    \
    file->usedTimes++;              \
    file->referenceBit = 1;

static File *evictFile(Filesystem *fs, File *toAdd)
{
    File *toEvict,
        *currFile;

    toEvict = fs->file_queue_head != toAdd ? fs->file_queue_head : fs->file_queue_head->next;

    if (fs->replacement_algo != FIFO)
    {
        currFile = fs->file_queue_head;
        while (currFile)
        {
            if (currFile != toAdd && (*replace_algo[fs->replacement_algo])(currFile, toEvict) > 0)
            {
                toEvict = currFile;
            }
            currFile = currFile->next;
        }
    }

    return toEvict;
}

/**
 * @brief Aggiunge al buffer 'buf' di dimensione corrente 'bufCurrSize' bytes un file.
 * Si assume la mutua esclusione sul file
 *
 * @param file file da aggiungere
 * @param buf buffer di file
 * @param bufCurrSize dimensione attuale del buffer
 * @return 0 successo, -1 altrimenti e errno settato
 */
static int copyFileToBuffer(File *file, char **buf, size_t *bufCurrSize)
{
    size_t path_len,
        bufNewSize;

    void *tmp;

    if (!file)
    {
        errno = EINVAL;
        return -1;
    }

    path_len = strlen(file->path) + 1;

    bufNewSize = *bufCurrSize + sizeof(size_t) + path_len + sizeof(size_t) + file->dataSize;

    tmp = realloc(*buf, bufNewSize);

    if (!tmp)
    {
        errno = ENOMEM;
        free(*buf);
        return -1;
    }

    *buf = tmp;

    memcpy(*buf + *bufCurrSize, &path_len, sizeof(size_t)); // Copio la dimensione del path del file

    memcpy(*buf + *bufCurrSize + sizeof(size_t), file->path, path_len); // Copio il path del file

    memcpy(*buf + *bufCurrSize + sizeof(size_t) + path_len, &(file->dataSize), sizeof(size_t)); // Copio la dimensione del file

    memcpy(*buf + bufNewSize - (file->dataSize), file->data, file->dataSize); // Copio il file

    *bufCurrSize = bufNewSize;

    return 0;
}
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

    newFile->prev = newFile->next = NULL;

    return newFile;
}

/**
 * @brief Dealloca il file puntato da 'filePtr'. Si assume che il chiamante abbia la mutua esclusione sul file.
 *
 * @param filePtr puntatore al file
 */
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

/**
 * @brief Rimuove il file dal filesystem 'fs' e salva la lista dei client che attendevano la lock sul puntatore 'signalForLock' che dovra' essere deallocata dal chiamante.
 * Si assume che il chiamante abbia la mutua esclusione sullo storage.
 *
 * @param fs puntatore al filesystem
 * @param file file da rimuovere
 * @param signalForLock puntatore alla lista di client che attendono la lock sul file
 */
static void deleteFile(Filesystem *fs, File *file, fdNode **signalForLock)
{
    LOCK(&(file->fileLock));

    while (file->nReaders > 0 || file->isWritten)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    if (file->prev)
        file->prev->next = file->next;
    else
        fs->file_queue_head = file->next;

    if (file->next)
        file->next->prev = file->prev;
    else
        fs->file_queue_tail = file->prev;

    if (file->waitingForLock && signalForLock)
    {
        *signalForLock = file->waitingForLock->head;
        file->waitingForLock->head = NULL;
    }

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

/**
 * @brief aggiunge un file al filesystem 'fs'. Si assume di avere la mutua esclusione sia sul filesytem che sul file.
 *
 * @param fs filesytem a cui aggiungere il file
 * @param file file da aggiungere
 * @return 0 se successo, -1 altrimenti e errno settato.
 */
static int addFile(Filesystem *fs, File *file)
{
    if (!fs || !file)
    {
        errno = EINVAL;
        return -1;
    }

    if (!icl_hash_insert(fs->hastTable, file->path, file))
    {
        errno = ENOMEM;
        return -1;
    }

    file->insertionTime = time(NULL); // Fifo
    file->lastUsed = time(NULL);      // Lru
    file->usedTimes = 1;              // Lfu
    file->referenceBit = 1;           // Second-chance

    if (!fs->file_queue_head)
    {
        fs->file_queue_head = file;
    }
    else
    {
        file->prev = fs->file_queue_tail;
        if (fs->file_queue_tail)
            fs->file_queue_tail->next = file;
    }

    fs->file_queue_tail = file;

    fs->currFiles++;
    fs->absMaxFiles = MAX(fs->absMaxFiles, fs->currFiles);

    return 0;
}

Filesystem *initFileSystem(size_t maxFiles, size_t maxMemory, int replacement_algo)
{
    int errnum;

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

    newFilesystem->maxMemory = maxMemory * 1000 * 1000;
    newFilesystem->currMemory = 0;
    newFilesystem->absMaxMemory = 0;

    newFilesystem->replacement_algo = replacement_algo;

    if ((errnum = pthread_mutex_init(&(newFilesystem->fileSystemLock), NULL)) != 0)
    {
        errno = errnum;
        return NULL;
    }

    newFilesystem->hastTable = icl_hash_create((int)(maxFiles * (0.75F)), NULL, NULL);
    if (!newFilesystem->hastTable)
    {
        pthread_mutex_destroy(&(newFilesystem->fileSystemLock));
        errno = ENOMEM;
        return NULL;
    }

    newFilesystem->file_queue_head = newFilesystem->file_queue_tail = NULL;

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

int openFileHandler(Filesystem *fs, const char *path, int openFlags, fdNode **signalForLock, int clientFd)
{
    File *file,
        *toEvict;

    if (!fs || !path || (clientFd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    int create = FLAG_ISSET(openFlags, O_CREATE);
    int lock = FLAG_ISSET(openFlags, O_LOCK);

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

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
        if (fs->currFiles == fs->maxFiles)
        {
            toEvict = evictFile(fs, NULL);
            deleteFile(fs, toEvict, signalForLock);
        }

        file = initFile(path);

        if (!file)
        {
            UNLOCK(&(fs->fileSystemLock));
            return -1;
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

int writeFileHandler(Filesystem *fs, const char *path, void *data, size_t dataSize, void **evicted_files, size_t *evicted_files_size, fdNode **signalForLock, int clientFd)
{
    File *file,
        *toEvict;

    char *fileData,
        *toEvict_buf = NULL;

    size_t toEvict_size = 0;

    fdNode *tmp_signalForLock;

    if (!fs || !path || !data || dataSize <= 0 || clientFd <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    LOCK(&(fs->fileSystemLock));

    file = getFile(fs, path);

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
        UNLOCK(&(fs->fileSystemLock));
        errno = EACCES;
        return -1;
    }

    // attendo finché non ci sono piu' lettori per scrivere il file
    while (file->nReaders > 0)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    file->isWritten = 1;

    UPDATE_FILE_REFERENCE(file);

    UNLOCK(&(file->fileLock));

    // I dati da scrivere sono troppi per lo storage
    if ((file->dataSize + dataSize) > fs->maxMemory)
    {
        errno = EFBIG;
        goto cleanup;
    }

    fileData = calloc((file->dataSize + dataSize), sizeof(*fileData));

    if (!fileData)
    {
        errno = ENOMEM;
        goto cleanup;
    }

    while (fs->currMemory + dataSize > fs->maxMemory)
    {
        toEvict = evictFile(fs, file);

        copyFileToBuffer(toEvict, &toEvict_buf, &toEvict_size);

        deleteFile(fs, toEvict, &tmp_signalForLock);

        concanateList(&((*signalForLock)), tmp_signalForLock);
    }

    *evicted_files = toEvict_buf;
    *evicted_files_size = toEvict_size;

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

    fs->currMemory += file->dataSize;
    fs->absMaxMemory = MAX(fs->absMaxMemory, fs->currMemory);

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

    UPDATE_FILE_REFERENCE(file);

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
        if (copyFileToBuffer(currFile, &file_data_buf, &bufCurrSize) == -1)
            break;

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
        CHECK_AND_ACTION(insertNode, ==, -1, perror("insertNode"); pthread_exit((void *)EXIT_FAILURE), file->waitingForLock, clientFd);
        UNLOCK(&(fs->fileSystemLock));
        return -2;
    }

    file->lockedBy = clientFd;

    UPDATE_FILE_REFERENCE(file);

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

    UPDATE_FILE_REFERENCE(file);

    BCAST(&(file->readWrite));

    UNLOCK(&(file->fileLock));

    return 0;
}

int removeFileHandler(Filesystem *fs, const char *path, fdNode **signalForLock, int clientFd)
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

    // Rimuovo l'fd del client dalla lista di quelli aperti.
    fdToClose = getNode(file->openedBy, clientFd);
    deleteNode(fdToClose);

    UPDATE_FILE_REFERENCE(file);

    BCAST(&(file->readWrite));

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

int clientExitHandler(Filesystem *fs, fdNode **signalForLock, int clientFd)
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
                nextNode->next = *signalForLock;
                (*signalForLock)->prev = nextNode;
                (*signalForLock) = nextNode;
                currFile->lockedBy = nextNode->fd;
            }
        }

        tmp = getNode(currFile->waitingForLock, clientFd);
        deleteNode(tmp);

        tmp = getNode(currFile->openedBy, clientFd);
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