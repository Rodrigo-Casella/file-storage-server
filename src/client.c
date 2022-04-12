#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>

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
    long nanosec = (long)((msec - (sec * 1000)) * 1000000); \
    timespec.tv_sec = sec;                                  \
    timespec.tv_nsec = nanosec;

#define ERROR_ARG_COPY(option, arg) \
    fprintf(stderr, "Errore copiando l'argomento '%s' di -%c.\n", arg, option);

#define INVALID_ARGUMENT(option, arg) \
    fprintf(stderr, "Errore opzione -%c: argomento '%s' non valido.\n", option, arg);

#define GET_N_ARGS(args, arg, n)               \
    char *token, *savePtr;                     \
    token = strtok_r(arg, ",", &savePtr);      \
    int i = 0;                                 \
    while (token)                              \
    {                                          \
        args[i++] = strdup(token);             \
        token = strtok_r(NULL, ",", &savePtr); \
    }

#define FREE_N_ARGS(args, n)    \
    for (int i = 0; i < n; i++) \
        if (args[i])            \
            free(args[i]);      \

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

int writeDirOnServer(char const *dirname, long const filesToWrite, int *filesWritten)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(dirname)))
    {
        perror("opendir");
        fprintf(stderr, "Errore aprendo directory: %s.\n", dirname);
        return -1;
    }

    char cwd[PATH_MAX];
    SYSCALL_EQ_RETURN(getcwd(cwd, PATH_MAX), NULL);

    if (strcmp(cwd, dirname))
        SYSCALL_EQ_RETURN(chdir(dirname), -1);

    //ciclo finché trovo entry oppure ho raggiunto il limite superiore di files da scrivere
    while ((errno = 0, entry = readdir(dir)) && (!filesToWrite || *filesWritten != filesToWrite))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        struct stat info;

        if (stat(entry->d_name, &info))
            continue;

        if (S_ISDIR(info.st_mode))
        {
            SYSCALL_EQ_RETURN(chdir(entry->d_name), -1);
            writeDirOnServer(".", filesToWrite, filesWritten);
            SYSCALL_EQ_RETURN(chdir(cwd), -1);
        }
        else
        {
            char path[PATH_MAX];
            SYSCALL_EQ_RETURN(realpath(entry->d_name, path), NULL);
            printf("scrivendo: %s\n", path);
            (*filesWritten)++;
        }
    }
    if (errno)
    {
        perror("readdir");
        SYSCALL_EQ_RETURN(closedir(dir), -1);
        return -1;
    }

    SYSCALL_EQ_RETURN(closedir(dir), -1);
    return 0;
}

int main(int argc, char *argv[])
{
    OptionList *list = parseCmdLine(argc, argv);

    if (!list)
    {
        fprintf(stderr, "Errore parsando linea di comando\n");
        exit(EXIT_FAILURE);
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
        ERROR_ARG_COPY(selectedOption->opt, selectedOption->arg);
        FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE);
    }
    freeOption(selectedOption);

    struct timespec requestInterval;
    long msec = 0; // valore di default

    if ((selectedOption = getOption(list, 't')))
    {
        if (isNumber(selectedOption->arg, &msec))
        {
            INVALID_ARGUMENT(selectedOption->opt, selectedOption->arg);
        }
        freeOption(selectedOption);
    }
    SET_TIMESPEC_MILLISECONDS(requestInterval, msec);

    int print = 0;
    if ((selectedOption = getOption(list, 'p')))
    {
        puts("Prints enables");
        print = 1;
        freeOption(selectedOption);
    }

    selectedOption = list->head;

    while (selectedOption)
    {
        switch (selectedOption->opt)
        {
        case 'w':
            char *args[2] = {NULL, NULL}; // args[0]: dirname, args[1]: numero file da scrivere
            GET_N_ARGS(args, selectedOption->arg, 2);

            long filesToWrite = 0;
            if (args[1] && (isNumber(args[1] + 2, &filesToWrite) || filesToWrite < 0)) // args[1] + 2: salto i caratteri n=
            {
                fprintf(stderr, "Errore nell'input del secondo argomento. Impostando valore di default.\n");
                filesToWrite = 0;
            }

            int filesWritten = 0;
            if (writeDirOnServer(args[0], filesToWrite, &filesWritten) || filesWritten < 0)
            {
                fprintf(stderr, "Non è stato possibile scrivere i file della directory %s sul server.\n", args[0]);
            }
            FREE_N_ARGS(args, 2);
            break;

        default:
            fprintf(stderr, "Errore opzione -%c gia' impostata.\n", selectedOption->opt); // opizioni -f -p o -t duplicate
            break;
        }
        selectedOption = selectedOption->next;
    }

    free(sockname);
    freeOptionList(list);
    return 0;
}
