#include <stdio.h>
#include <stdlib.h>

#include "../include/cmdLineParser.h"

int main(int argc, char *argv[])
{
    OptionList *list = parseCmdLine(argc, argv);

    if (!list) {
        fprintf(stderr, "Errore parsando linea di comando\n");
    }

    freeOptionList(list);
    return 0;
}
