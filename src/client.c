#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "../include/cmdLineParser.h"
#include "../include/utils.h"

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

#define SET_TIMESPEC_MILLISECONDS(timespec, msec)           \
    int sec = (int)msec / 1000;                             \
    long nanosec = (long)((msec - (sec * 1000) )* 1000000); \
    timespec.tv_sec = sec;                                  \
    timespec.tv_nsec = nanosec;

int copyOptionArg(Option *option, char **dest)
{
    if (!option)
    {
        fprintf(stderr, "Errore, option e' NULL.\n");
        return -1;
    }

    *dest = strdup(option->arg);

    if (!(*dest))
    {
        perror("stdup");
        return -1;
    }

    return 0;
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

    char *sockname = NULL;

    if (!(selectedOption = getOption(list, 'f')))
    {
        fprintf(stderr, "Errore, opzione -f necessaria.\n");
        FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE);
    }

    if (copyOptionArg(selectedOption, &sockname))
    {
        fprintf(stderr, "Errore copiando il nome del socketfile");
        FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE);
    }
    freeOption(selectedOption);

    struct timespec requestInterval;
    SET_TIMESPEC_MILLISECONDS(requestInterval, 0); // 0 valore di default

    if ((selectedOption = getOption(list, 't')))
    {
        char *timeInterval = NULL;
        if (copyOptionArg(selectedOption, &timeInterval))
        {
            fprintf(stderr, "Errore copiando tempo di intervallo fra richieste. Verra' usato il valore predefinito.\n");
        }
        else
        {
            long msec;
            if (isNumber(timeInterval, &msec))
            {
                if (!errno)
                    perror("isNumber");

                fprintf(stderr, "Errore, l'argomento passato %s non è un numero valido. Verra' usato il valore predefinito.\n", timeInterval);
            }
            free(timeInterval);
            SET_TIMESPEC_MILLISECONDS(requestInterval, msec);
        }
        freeOption(selectedOption);
    }

    free(sockname);
    freeOptionList(list);
    return 0;
}
