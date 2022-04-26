#ifndef API_H
#define API_H

extern int toPrint;
extern int fd_skt;
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
#endif