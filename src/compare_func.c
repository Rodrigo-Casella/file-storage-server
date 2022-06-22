#include "../include/define_source.h"

#include "../include/compare_func.h"

int fifo(File *file1, File *file2)
{
    return (file2->insertionTime - file1->insertionTime);
}

int lru(File *file1, File *file2)
{
    return (file2->lastUsed - file1->lastUsed);
}

int lfu(File *file1, File *file2)
{
    return (file2->usedTimes - file1->usedTimes);
}

int second_chance(File *file1, File *file2)
{
    if (file2->insertionTime - file1->insertionTime > 0)
    {
        if (file1->referenceBit)
        {
            file1->referenceBit = 0;
            file1->insertionTime = time(NULL);
            return 0;
        }
    }

    return 1;
}

int (*replace_algo[4])(File* file1, File* file2) = {fifo, lru, lfu, second_chance};