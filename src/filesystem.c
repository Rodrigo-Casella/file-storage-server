#include "../include/define_source.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../include/filesystem.h"
#include "../include/utils.h"
#include "../include/mutex.h"
#include "../include/definitions.h"

static fdNode *initNode(int fd)
{
    fdNode *newNode;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newNode, perror("calloc"); return NULL, 1, sizeof(*newNode));

    newNode->fd = fd;
    newNode->next = newNode->prev = NULL;

    return newNode;
}

static void deleteNode(fdNode *node)
{
    if (node)
        free(node);
}

static fdList *initList()
{
    fdList *newList;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newList, perror("calloc"); return NULL, 1, sizeof(*newList));

    newList->head = newList->tail = NULL;

    return newList;
}

static void deleteList(fdList *list)
{
    fdNode *tmp;

    while (list->head)
    {
        tmp = list->head;
        list->head = list->head->next;
        deleteNode(tmp);
    }

    free(list);
}

static fdNode *getNode(fdList *list, int key)
{
    fdNode *curr = list->head;

    while (curr)
    {
        if (curr->fd == key)
        {
            if (!curr->prev)
            {
                list->head = curr->next;
            }
            else
            {
                curr->prev->next = curr->next;
                curr->next->prev = curr->prev;
            }

            curr->next = curr->prev = NULL;
            return curr;
        }
    }
    return NULL;
}

/**
 * @brief Cerca il nodo con la chive 'key' nella lista
 *
 * @param list lista in cui cercare
 * @param key chiave del nodo
 * \retval 1 se ho trovato il nodo
 * \retval 0 se non ho trovato il nodo
 */
static int findNode(fdList *list, int key)
{
    fdNode *curr = list->head;

    while (curr)
    {
        if (curr->fd == key)
            return 1;

        curr = curr->next;
    }

    return 0;
}

static int insertNode(fdList *list, int fd)
{
    if (!list || (fd <= 0))
    {
        errno = EINVAL;
        return -1;
    }

    fdNode *newNode = initNode(fd);

    if (!newNode)
        return -1;

    if (!list->head)
    {
        list->head = newNode;
    }
    else
    {
        newNode->prev = list->tail;
        if (list->tail)
            list->tail->next = newNode;
    }

    list->tail = newNode;
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
    char *fileName;
    File *file;
    int i;
    icl_entry_t *entry;
    icl_hash_foreach(fs->hastTable, i, entry, fileName, file, printf("File: %s\n", fileName));
}

/**
 * @brief apre un file del filesystem.
 *
 * @param fs puntatore al filesystem
 * @param path path del file da aprire
 * @param openFlags flags da settare aprendo il file
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
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
                         UNLOCK(&(fs->fileSystemLock)); return -1, file->openedBy, clientFd);
    }

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));

    return 0;
}

/**
 * @brief Scrive un file nel filesystem. (Le scritture avvengono solo in append)
 *
 * @param fs puntatore al filesystem
 * @param path path del file da scrivere
 * @param data puntatore hai dati da scrivere
 * @param dataSize dimensione dei dati da scrivere
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
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

    // il file è in stato di lock
    if ((file->lockedBy && file->lockedBy != clientFd))
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EACCES;
        return -1;
    }

    // il client non ha aperto il file
    if (!findNode(file->openedBy, clientFd))
    {
        UNLOCK(&(file->fileLock));
        UNLOCK(&(fs->fileSystemLock));
        errno = EBADF;
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

    // attendo finché non ci sono lettori per scrivere il file
    while (file->nReaders > 0)
    {
        WAIT(&(file->readWrite), &(file->fileLock));
    }

    file->isWritten = 1;

    UNLOCK(&(file->fileLock));

    char *fileData;

    while (fs->currMemory + dataSize > fs->maxMemory)
    {
        // TODO: Expell file from storage
    }

    fileData = calloc((file->dataSize + dataSize), sizeof(*fileData));

    if (!fileData)
    {
        LOCK(&(file->fileLock));
        file->isWritten = 0;
        UNLOCK(&(file->fileLock));

        UNLOCK(&(fs->fileSystemLock));

        return -1;
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

    LOCK(&(file->fileLock));

    file->isWritten = 0;

    UNLOCK(&(file->fileLock));
    UNLOCK(&(fs->fileSystemLock));
    return 0;
}

/**
 * @brief chiude un file del filesystem.
 *
 * @param fs puntatore al filesystem
 * @param path path del file da chiudere
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
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