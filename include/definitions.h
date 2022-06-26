#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#define  O_CREATE 0x01 // 01
#define O_LOCK 0x02 // 10

#define FLAG_ISSET(flag, bit) (((flag) & (bit)) == (bit))
#endif