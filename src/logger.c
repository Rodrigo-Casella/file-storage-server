#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "../include/logger.h"
#include "../include/utils.h"
#include "../include/boundedqueue.h"

/**
 * @brief scrive le operazioni eseguite dal server sul file di log
 * 
 * @param args argomenti del thread del logger: puntatore allo storage, percorso del file di log
 * @return void* 
 */
void *writeLogToFile(void *args) {
    FileLogger *logger_args = (FileLogger*)args;
    Filesystem *fs = logger_args->filesystem;

    FILE* logFile;
    CHECK_RET_AND_ACTION(fopen, ==, NULL, logFile, perror("fopen"); pthread_exit((void*)EXIT_FAILURE), logger_args->logFilePath, "w"); //apro il file di log in scrittura

    char *log_buf; //buffer per il log delle operazioni

    while (1) {
        char* logFromStorage;
        CHECK_RET_AND_ACTION(pop, ==, NULL, logFromStorage, perror("pop"); pthread_exit((void*)EXIT_FAILURE), fs->logger_msg_queue); //estraggo i messaggi di log dalla coda dello storage
        
        if (strncmp(logFromStorage, STOP_MSG, strlen(STOP_MSG)) == 0) { //se il messaggio è quello di "STOP" allora mi fermo
            free(logFromStorage);
            break;
        }

        log_buf = calloc(strlen(logFromStorage) + 1, sizeof(char));

        if (!log_buf)
        {
            errno = ENOMEM;
            perror("calloc");
            free(logFromStorage);
            break;
        }
        memcpy(log_buf, logFromStorage, strlen(logFromStorage) + 1); //copio il messaggio sul buffer

        if (fputs(log_buf, logFile) == EOF)
        {
            perror("fputs");
            goto cleanup;
        }

        if (fflush(logFile) == EOF) // e lo scrivo sul file
        {
            perror("fflush");
            goto cleanup;
        }

        cleanup:
        free(logFromStorage);
        free(log_buf);

        if (errno)
            break;
    }

    fclose(logFile); //quando ho finito chiudo il file di log

    pthread_exit(errno ? (void*)EXIT_FAILURE : NULL);
}