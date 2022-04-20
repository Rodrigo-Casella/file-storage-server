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

#define SET_TIMESPEC_MILLISECONDS(timespec, msec)                                        \
    int sec = (int)msec / 1000;                                                          \
    long nanosec = (long)((msec - (sec * 1000)) * 1000000);                              \
    if (nanosec > 999999999)                                                             \
    {                                                                                    \
        fprintf(stderr, "Tempo intervallo tra richieste in nanosecondi fuori range.\n"); \
        sec = 0, nanosec = 0;                                                            \
    }                                                                                    \
    timespec.tv_sec = sec;                                                               \
    timespec.tv_nsec = nanosec;

#define INVALID_ARGUMENT(option, arg) \
    fprintf(stderr, "Errore opzione -%c: argomento '%s' non valido.\n", option, arg);

#define GET_N_ARGS(arg, del, ...)                           \
    if (1)                                                  \
    {                                                       \
        char *token, *savePtr;                              \
        char **ptrArr[] = {__VA_ARGS__};                    \
        int arr_length = 0;                                 \
        ARRAY_LENGTH(*ptrArr, arr_length);                  \
        token = strtok_r(arg, del, &savePtr);               \
        for (int i = 0; token && i <= arr_length; i++)      \
        {                                                   \
            *ptrArr[i] = strndup(token, strlen(token) + 1); \
            token = strtok_r(NULL, del, &savePtr);          \
        }                                                   \
    }

#define FREE_N_ARGS(...)                      \
    if (1)                                    \
    {                                         \
        char **ptrArr[] = {__VA_ARGS__};      \
        int arr_length = 0;                   \
        ARRAY_LENGTH(*ptrArr, arr_length);    \
        for (int i = 0; i <= arr_length; i++) \
            if (*ptrArr[i])                   \
                free(*ptrArr[i]);             \
    }

int copyOptionArg(Option *option, char **dest)
{
    if (!option)
    {
        fprintf(stderr, "Errore, option e' NULL.\n");
        return -1;
    }

    *dest = strndup(option->arg, strlen(option->arg) + 1);

    if (!(*dest))
    {
        perror("strndup");
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

    // ciclo finché trovo entry oppure ho raggiunto il limite superiore di files da scrivere
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

    CHECK_RET_AND_ACTION(getOption, ==, NULL, selectedOption, fprintf(stderr, "Errore, opzione -f necessaria.\n");
                         FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), list, 'f');

    CHECK_AND_ACTION(copyOptionArg, ==, -1, FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), selectedOption, &sockname);

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
        case 'w':;
            char *dirname = NULL, *nFiles = NULL;
            GET_N_ARGS(selectedOption->arg, ",", &dirname, &nFiles);

            long filesToWrite = 0;
            if (nFiles && (isNumber(nFiles + 2, &filesToWrite) || filesToWrite < 0)) // nFiles + 2: salto i caratteri n=
            {
                fprintf(stderr, "Errore nell'input del secondo argomento. Impostando valore di default.\n");
                filesToWrite = 0;
            }

            int filesWritten = 0;
            if (writeDirOnServer(dirname, filesToWrite, &filesWritten) || filesWritten < 0)
            {
                fprintf(stderr, "Non è stato possibile scrivere i file della directory %s sul server.\n", dirname);
            }
            FREE_N_ARGS(&dirname, &nFiles);
            break;
        case 'W':;
            char *files = NULL;
            CHECK_AND_ACTION(copyOptionArg, ==, -1, break, selectedOption, &files);

            TOKENIZER(files, ",", char path[PATH_MAX];
                      SYSCALL_EQ_ACTION(realpath, NULL, fprintf(stderr, "Non è stato possibile risolvere il path di %s.\n", token); continue, token, path);
                      printf("scrivendo: %s\n", path);)

            free(files);
            break;
        default:
            fprintf(stderr, "Errore opzione -%c gia' impostata.\n", selectedOption->opt); // opizioni -f -p o -t duplicate
            break;
        }

        nanosleep(&requestInterval, NULL);
        selectedOption = selectedOption->next;
    }

    free(sockname);
    freeOptionList(list);
    return 0;
}
