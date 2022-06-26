#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#define OPEN_FILE 1
#define WRITE_FILE 2
#define APPEND_FILE 3
#define READ_FILE 4
#define READ_N_FILE 5
#define LOCK_FILE 6
#define UNLOCK_FILE 7
#define REMOVE_FILE 8
#define CLOSE_FILE 9

#define SUCCESS 1 // operazione terminata con successo
#define INVALID_REQ 2 // richiesta invalida
#define FILENOENT 3 // il file non esiste
#define FILEEX 4 // il file esiste di gia'
#define SERVER_ERR 5 // errore del server
#define FILE_LOCK 6 // file in stato di lock
#define BIG_FILE 7 // file troppo grande per essere salvato sul server
#define INVALID_RES 8 // risposta invalida dal server

#define CLIENT_LEFT_MSG "0000"
#endif