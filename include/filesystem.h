#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <pthread.h>
#include <time.h>

#include "../include/fdList.h"
#include "../include/icl_hash.h"

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

/**
 * @brief Alloco e inizializzo un filesystem.
 *
 * @param maxFiles numero massimo di file che possono essere memorizzati nel filesystem
 * @param maxMemory spazio massimo che può occupare il filesystem (in bytes)
 *
 * \retval NULL se c'è stato un errore allocando o inizializzando il filesystem (errno settato)
 * \retval newFilesystem puntatore al filesystem allocato
 */
Filesystem *initFileSystem(long maxFiles, long maxMemory);
void deleteFileSystem(Filesystem *fs);
void printFileSystem(Filesystem *fs);

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
int openFileHandler(Filesystem *fs, const char *path, int openFlags, int clientFd);

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
int writeFileHandler(Filesystem *fs, const char *path, void *data, size_t dataSize, int clientFd);

/**
 * @brief Legge un file dal filesystem
 * 
 * @param fs puntatore al filesystem
 * @param path path del file da leggere
 * @param data_buf buffer su cui scrivere il file
 * @param dataSize dimensione del file letto
 * @param clientFd fd del processo che ha richiesto l'operazione
 * 
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int readFileHandler(Filesystem *fs, const char *path, void **data_buf, size_t *dataSize, int clientFd);

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
int closeFileHandler(Filesystem *fs, const char *path, int clientFd);
void addDummyFiles(Filesystem *fs);
#endif