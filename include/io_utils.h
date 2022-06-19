#ifndef IO_UTILS_H
#define IO_UITLS_H

/**
 * @brief Scrive il file 'path' di 'size' bytes contenuti nel buffer 'buf' su disco.
 * 
 * @param path path del file da scrivere 
 * @param buf buffer contenente i dati da scrivere
 * @param size dimensione in bytes del buffer 'buf'
 * @return 0 se successo, -1 altrimenti e errno settato
 */
int writeFileToDisk(const char *path, void *buf, size_t size);

/**
 * @brief Scrive nella cartella 'save_dir' i 'file_size' bytes del buffer 'file_data' con pathname 'file_path'.
 * 
 * @param save_dir cartella di salvataggio in memoria secondaria
 * @param file_path pathname del file da scrivere
 * @param file_data buffer con i dati da scrivere
 * @param file_size dimensione del buffer
 * @return 0 se successo, -1 altrimenti e ernno settato
 */
int writeFileToDir(const char* save_dir, const char *file_path, void *file_data, size_t file_size);

/**
 * @brief Legge il file di percorso 'path' e salva in 'file_len' il numero di bytes letti.
 * 
 * @param path percorso del file da leggere
 * @param file_len bytes letti
 * @return puntatore al buffer di 'file_len' bytes dei dati del file, NULL altrimenti e errno settato
 */
char *readFileFromPath(const char *path, size_t *file_len);

/**
 * @brief Legge dal socket 'fd_skt' e salva in 'file_len' il numero di bytes letti.
 * 
 * @param fd_skt socket da cui leggere i dati
 * @param file_len bytes letti
 * @return puntatore al buffer dei dati letti dalla socket 
 */
char *readFileFromServer(int fd_skt, size_t *file_len);

/**
 * @brief Legge fino a n file dal server, se n <= 0 allora legge tutti i file presenti sul server e li salva in save_dir (opzionale).
 *
 * @param n limite superio di file da leggere
 * @param save_dir cartella in memoria secondaria su cui salvare i file letti
 * @return il numero di file letti dal server, -1 se c'è stato un errore.
 */
int readMultipleFilesFromServer(int fd_skt, int n, const char *save_dir);

#endif