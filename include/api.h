#ifndef API_H
#define API_H

extern int toPrint;
extern int fd_skt;

/**
 * @brief Scrive il file 'path' di 'size' bytes contenuti nel buffer 'buf' su disco.
 * 
 * @param path path del file da scrivere 
 * @param buf buffer contenente i dati da scrivere
 * @param size dimensione in bytes del buffer 'buf'
 * @return 0 se successo, -1 altrimenti e errno settato
 */
int writeFileToDisk(char *path, void *buf, size_t size);
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* pathname, int flags);
int writeFile(const char* pathname, const char* dirname);
int readFile(const char* pathname, void** buf, size_t* size);
int closeFile(const char* pathname);
#endif