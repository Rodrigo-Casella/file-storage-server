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
    fdList *waitingForLock;
    int lockedBy;

    time_t insertionTime; // FIFO
    time_t lastUsed; // LRU
    size_t usedTimes; // LFU
    short referenceBit; // Second-chance

    struct file *prev;
    struct file *next;
} File;

typedef struct filesystem
{
    size_t maxFiles;
    size_t currFiles;
    size_t absMaxFiles;

    size_t maxMemory;
    size_t currMemory;
    size_t absMaxMemory;

    icl_hash_t *hastTable;
    File *file_queue_head; // FIFO e Second-chance
    File *file_queue_tail;

    int replacement_algo;

    pthread_mutex_t fileSystemLock;
} Filesystem;

/**
 * @brief Alloca e inizializza un filesystem con capacita' massima di 'maxFiles' files e una memoria massima di 'maxMemory' Mbytes.
 *
 * @param maxFiles numero massimo di file che possono essere memorizzati nel filesystem
 * @param maxMemory spazio massimo che può occupare il filesystem (in Mbytes)
 *
 * \retval NULL se c'è stato un errore (errno settato)
 * \retval newFilesystem puntatore al filesystem allocato
 */
Filesystem *initFileSystem(size_t maxFiles, size_t maxMemory, int replacement_algo);

/**
 * @brief Dealloca il filesystem 'fs'. Si assume che il chiamante abbia la mutua esclusione sul filesystem.
 * 
 * @param fs puntatore al filesystem
 */
void deleteFileSystem(Filesystem *fs);

/**
 * @brief Stampa il path e la dimensione di tutti i file presenti nel filesytem. Si assume che il chiamante abbia la mutua esclusione sul filesystem.
 * 
 * @param fs puntatore al filesystem
 */
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
int openFileHandler(Filesystem *fs, const char *path, int openFlags, fdNode **signalForLock, int clientFd);

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
int writeFileHandler(Filesystem *fs, const char *path, void *data, size_t dataSize, void **evicted_files, size_t *evicted_files_size, fdNode **signalForLock, int clientFd);

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
 * @brief Legge fino a 'upperLimit' files dal server e li salva nel buffer 'data_buf' di dimensione 'dataSize'
 *
 * @param fs puntatore al filesystem
 * @param upperLimit limite superiore di file da leggere
 * @param data_buf buffer su cui scrivere il file
 * @param dataSize dimensione del buffer
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval numero dei file letti >= 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int readNFilesHandler(Filesystem *fs, const int upperLimit, void **data_buf, size_t *dataSize);

/**
 * @brief richiesta di acquisire la lock sul file 'path' del filesystem 'fs' da parte del processo 'clientFd'.
 *
 * @param fs puntatore al filesystem
 * @param path path del file su cui acquisire la lock
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int lockFileHandler(Filesystem *fs, const char *path, int clientFd);

/**
 * @brief richiesta di rilasciare la lock sul file 'path' del filesystem 'fs' da parte del processo 'clientFd'.
 *
 * @param fs puntatore al filesystem
 * @param path path del file su cui rilasciare la lock
 * @param nextLockFd puntatore al fd del client che ha acquisito la lock
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int unlockFileHandler(Filesystem *fs, const char *path, int *nextLockFd, int clientFd);

/**
 * @brief richiesta di rimuovere il file 'path' dal filesystem 'fs' da parte del processo 'clientFd'. In 'signalForLock' viene ritornato il puntatore alla lista dei client in attesa per la lock
 *
 * @param fs puntatore al filesystem
 * @param path path del file da rimuovere
 * @param signalForLock lista dei client in attesa della lock per il file
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int removeFileHandler(Filesystem *fs, const char *path, fdNode **signalForLock, int clientFd);

/**
 * @brief richiesta di chiudere il file 'path' dal filesystem 'fs' da parte del processo 'clientFd'.
 *
 * @param fs puntatore al filesystem
 * @param path path del file da chiudere
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int closeFileHandler(Filesystem *fs, const char *path, int clientFd);

/**
 * @brief 
 * 
 * @param fs puntatore al filesystem
 * @param notifyList lista dei client in attesa della lock per il file
 * @param clientFd fd del processo che ha richiesto l'operazione
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int clientExitHandler(Filesystem *fs, fdNode **signalForLock, int clientFd);

/**
 * @brief Verifica che il client che ha richiesto una writeFile abbia precedentemente effettuato una openFile con i flag O_CREATE O_LOCK
 *
 * @param fs puntatore al filesystem
 * @param path path del file
 * @param clientFd fd del processo che ha richiesto l'operazione
 *
 * \retval 0 se successo
 * \retval -1 se errore (errno settato opportunatamente)
 */
int canWrite(Filesystem *fs, const char *path, int clientFd);
#endif