#ifndef COMPARE_FUNC_H
#define COMPARE_FUNC_H

#include "../include/filesystem.h"

#define FIFO 0
#define LRU 1
#define LFU 2
#define SEC 3

int fifo(File *file1, File *file2);

int lru(File *file1, File *file2);

int lfu(File *file1, File *file2);

int second_chance(File *file1, File *file2);

extern int (*replace_algo[4])(File* file1, File* file2);

#endif