#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#define O_CREATE 1 // 01
#define O_LOCK 2 // 10

#define FLAG_ISSET (bitwise, flag) (((bitwise) & (flag)) == (flag))
#endif