#ifndef LOGGER_H
#define LOGGER_H

#define LOGBUFSIZE 1024
#include "../include/filesystem.h"
#include <linux/limits.h>

#define STOP_MSG "STOP"

typedef struct fileLogger {
    char logFilePath[PATH_MAX]; //percorso del file di log
    Filesystem *filesystem; //puntatore al filesystem
} FileLogger;

void *writeLogToFile(void *args);
#endif