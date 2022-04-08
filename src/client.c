#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cmdLineParser.h"

#define HELP_MSG                                                                                                                        \
    "Uso: %s -f <socketfile> [OPTIONS]\n"                                                                                               \
    "-h,                    stampa questo messaggio e termina immediatamente.\n"                                                        \
    "-f <sockefile>,        specifica la socket a cui connettersi.\n"                                                                   \
    "-w <dirname>[,n=0],    scrive sul server i file presenti nella cartella specificata;\n"                                            \
    "                       se la cartella contiene altre sotto cartelle queste\n"                                                      \
    "                       vengono visitate ricorsivamente e vengono scritti fino a N file, se N non e' specificato\n"                 \
    "                       oppure e' uguale a 0 allora vengono scritti tutti i file incontrati\n"                                      \
    "-W <file1>[,file2],    scrive sul server i file specificati separati da','.\n"                                                     \
    "-D <dirname>,          specifica la cartella in cui salvare i file eventualmente espulsi dal server a seguito di una scrittura.\n" \
    "                       Da usare congiuntamente a -w o -W.\n"                                                                       \
    "-r <file1>[,file2],    specifica i file da leggere sul server separati da ','.\n"                                                  \
    "-R [n=0],              legge dal server fino ad N file, se N non e' specificato oppure e' uguale a 0\n"                            \
    "                       vengono letti tutti i file presenti sul server.\n"                                                          \
    "-d <dirname>,          specifica la cartella in cui salvare i file letti dal server. Da usare congiuntamente a -r o -R.\n"         \
    "-t <time>,             tempo in millisecondi che intercorre tra due richieste successive al server.\n"                             \
    "-l file1[,file2],      lista di file su cui acquisire la mutua esclusione separati da ','.\n"                                      \
    "-u file1[,file2],      lista di file su cui rilasciare la mutua esclusione separati da ','.\n"                                     \
    "-c file1[,file2],      lista di file da rimuovere dal server separati da ','.\n"                                                   \
    "-p,                    abilita le stampe sullo standard output per ogni operazione.\n"

#define FREE_AND_EXIT(list, option, status) \
    freeOption(option);                     \
    freeOptionList(list);                   \
    exit(status);

#define PRINT_HELP_MSG_AND_EXIT(list, option) \
    printf(HELP_MSG, argv[0]);                \
    FREE_AND_EXIT(list, option, EXIT_SUCCESS);

char *copyOptionArg(Option *option)
{
    size_t argLength = strlen(option->arg) + 1;
    char *arg = malloc(sizeof(char) * argLength);

    if (!arg)
    {
        perror("arg");
        return NULL;
    }

    strncpy(arg, option->arg, argLength);
    return arg;
}

int main(int argc, char *argv[])
{
    OptionList *list = parseCmdLine(argc, argv);

    if (!list)
    {
        fprintf(stderr, "Errore parsando linea di comando\n");
    }

    Option *selectedOption;

    if ((selectedOption = getOption(list, 'h')))
    {
        PRINT_HELP_MSG_AND_EXIT(list, selectedOption);
    }
    freeOption(selectedOption);

    char *sockname;

    if (!(selectedOption = getOption(list, 'f')))
    {
        fprintf(stderr, "Errore, opzione -f necessaria.\n");
        FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE);
    }

    if (!(sockname = copyOptionArg(selectedOption)))
    {
        fprintf(stderr, "Errore copiando il nome del socketfile");
        FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE);
    }
    freeOption(selectedOption);

    free(sockname);
    freeOptionList(list);
    return 0;
}
