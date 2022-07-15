#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <pthread.h>
#include <time.h>

#include "../include/fdList.h"
#include "../include/icl_hash.h"
#include "../include/boundedqueue.h"

#define LOGGER_MSG_QUEUE_LEN 20

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

    size_t evictedFiles;

    BQueue_t *logger_msg_queue;

    pthread_mutex_t fileSystemLock;
} Filesystem;

/**
 * @brief Costruisce un messaggio di log con i vari parametri e lo inserisce nella coda dei messaggi per il logger 'logger_msg_queue'
 *
 * @param logger_msg_queue coda dei messaggi per il thread logger
 * @param op nome operazione
 * @param pathname path del file su cui e' stata eseguita l'operazione
 * @param clientFd fd del client che ha richiesto l'operazione
 * @param dataSize bytes scritti/letti
 * @return 0 se successo, -1 altrimenti e errno settato
 */
int logOperation(BQueue_t *logger_msg_queue, const char *op, const char *pathname, const int clientFd, const size_t dataSize);

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
int openFileHandler(Filesystem *fs, const char *path, int openFlags, fdList **signalForLock, int clientFd);

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
int writeFileHandler(Filesystem *fs, const char *path, void *data, size_t dataSize, void **evicted_files, size_t *evicted_files_size, fdList **signalForLock, int clientFd);

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
int readNFilesHandler(Filesystem *fs, const int upperLimit, void **data_buf, size_t *dataSize, int clientFd);

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
int removeFileHandler(Filesystem *fs, const char *path, fdList **signalForLock, int clientFd);

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
int clientExitHandler(Filesystem *fs, fdList **signalForLock, int clientFd);

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