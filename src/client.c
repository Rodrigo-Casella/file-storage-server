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

#define INVALID_ARGUMENT(option, arg) fprintf(stderr, "Errore opzione -%c: argomento '%s' non valido.\n", option, arg)

#define GET_N_ARGS(arg, del, ...)                           \
    if (1)                                                  \
    {                                                       \
        char *token, *savePtr;                              \
        char **ptrArr[] = {__VA_ARGS__};                    \
        int arr_length = 0;                                 \
        ARRAY_LENGTH(*ptrArr, arr_length);                  \
        token = strtok_r(arg, del, &savePtr);               \
        for (int i = 0; token && i < arr_length; i++)       \
        {                                                   \
            *ptrArr[i] = strndup(token, strlen(token) + 1); \
            token = strtok_r(NULL, del, &savePtr);          \
        }                                                   \
    }

#define FREE_N_ARGS(...)                     \
    if (1)                                   \
    {                                        \
        char **ptrArr[] = {__VA_ARGS__};     \
        int arr_length = 0;                  \
        ARRAY_LENGTH(*ptrArr, arr_length);   \
        for (int i = 0; i < arr_length; i++) \
            if (*ptrArr[i] != NULL)          \
                free(*ptrArr[i]);            \
    }

#define CHECK_SAVE_DIR(currOption, nextOption, saveDir, skip)                                                       \
    if (currOption->next && currOption->next->opt == nextOption)                                               \
        skip = 1;                                                                                                      \
                                                                                                                       \
    if (skip)                                                                                                          \
        CHECK_AND_ACTION(copyOptionArg, ==, -1,                                                                        \
                         fprintf(stderr, "Errore copiando cartella di salvataggio: %s.\n", currOption->next->arg); \
                         , currOption->next, &saveDir);

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

int writeDirOnServer(char const *dirToWrite, char const *dirToSave, long const filesToWrite, int *filesWritten)
{
    DIR *dir;
    struct dirent *entry;

    SYSCALL_RET_EQ_ACTION(opendir, NULL, dir, fprintf(stderr, "Errore aprendo directory: %s.\n", dirToWrite); return -1, dirToWrite);

    char cwd[PATH_MAX];
    SYSCALL_EQ_RETURN(getcwd, NULL, cwd, PATH_MAX);

    if (strcmp(cwd, dirToWrite))
        SYSCALL_EQ_RETURN(chdir, -1, dirToWrite);

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
            SYSCALL_EQ_RETURN(chdir, -1, entry->d_name);
            writeDirOnServer(".", dirToSave, filesToWrite, filesWritten);
            SYSCALL_EQ_RETURN(chdir, -1, cwd);
        }
        else
        {
            char path[PATH_MAX];
            SYSCALL_EQ_RETURN(realpath, NULL, entry->d_name, path);
            printf("scrivendo: %s\n", path);
            (*filesWritten)++;
        }
    }
    if (errno)
    {
        perror("readdir");
        SYSCALL_EQ_RETURN(closedir, -1, dir);
        return -1;
    }

    SYSCALL_EQ_RETURN(closedir, -1, dir);
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

    if ((selectedOption = getOption(list, 'h'))) {
        PRINT_HELP_MSG_AND_EXIT(list, selectedOption);
    }

    freeOption(selectedOption);

    char *sockname = NULL;

    // CHECK_RET_AND_ACTION(getOption, ==, NULL, selectedOption, fprintf(stderr, "Errore, opzione -f necessaria.\n"), list, 'f');
    CHECK_RET_AND_ACTION(getOption, ==, NULL, selectedOption, fprintf(stderr, "Errore, opzione -f necessaria.\n"); FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), list, 'f');

    CHECK_AND_ACTION(copyOptionArg, ==, -1, FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), selectedOption, &sockname);

    freeOption(selectedOption);

    struct timespec requestInterval;
    long msec = 0; // valore di default

    if ((selectedOption = getOption(list, 't')))
    {
        if (isNumber(selectedOption->arg, &msec))
            INVALID_ARGUMENT(selectedOption->opt, selectedOption->arg);

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
    int skipOption = 0;
    char *saveDir = NULL;

    while (selectedOption)
    {
        switch (selectedOption->opt)
        {
        case 'w':;
            char *dirToWrite = NULL, *nFilesToWrite_string = NULL;

            CHECK_SAVE_DIR(selectedOption, 'D', saveDir, skipOption);

            GET_N_ARGS(selectedOption->arg, ",", &dirToWrite, &nFilesToWrite_string);

            long nFilesToWrite = 0;
            if (nFilesToWrite_string && (isNumber(nFilesToWrite_string + 2, &nFilesToWrite) || nFilesToWrite < 0)) // nFiles + 2: salto i caratteri n=
            {
                fprintf(stderr, "Errore nell'input del secondo argomento. Impostando valore di default.\n");
                nFilesToWrite = 0;
            }

            int filesWritten = 0;
            if (writeDirOnServer(dirToWrite, saveDir, nFilesToWrite, &filesWritten) || filesWritten < 0)
                fprintf(stderr, "Non è stato possibile scrivere i file della directory %s sul server.\n", dirToWrite);

            FREE_N_ARGS(&dirToWrite, &nFilesToWrite_string, &saveDir);
            break;
        case 'W':;
            char *filesToWrite = NULL;
            CHECK_SAVE_DIR(selectedOption, 'D', saveDir, skipOption);
            CHECK_AND_ACTION(copyOptionArg, ==, -1, break, selectedOption, &filesToWrite);

            TOKENIZER(filesToWrite, ",", char path[PATH_MAX];
                      SYSCALL_EQ_ACTION(realpath, NULL, fprintf(stderr, "Non è stato possibile risolvere il path di %s.\n", token); continue, token, path);
                      printf("scrivendo: %s\n", path);)

            FREE_N_ARGS(&filesToWrite, &saveDir);
            break;
        case 'D':
            fprintf(stderr, "Errore, l'opzione -D va usata congiuntamente a -w o -W.\n");
            break;
        case 'r':;
            char *filesToRead = NULL;
            CHECK_SAVE_DIR(selectedOption, 'd', saveDir, skipOption);
            CHECK_AND_ACTION(copyOptionArg, ==, -1, break, selectedOption, &filesToRead);

            TOKENIZER(filesToRead, ",", printf("leggendo: %s\n", token))

            FREE_N_ARGS(&filesToRead, &saveDir);
            break;
        case 'R':;
            char *nFilesToRead_string = NULL;
            CHECK_SAVE_DIR(selectedOption, 'd', saveDir, skipOption);

            GET_N_ARGS(selectedOption->arg, ",", &nFilesToRead_string);

            long nFilesToRead = 0;
            if (nFilesToRead_string && (isNumber(nFilesToRead_string + 2, &nFilesToRead) || nFilesToRead < 0)) // nFiles + 2: salto i caratteri n=
            {
                fprintf(stderr, "Errore nell'input del secondo argomento. Impostando valore di default.\n");
                nFilesToRead = 0;
            }
            printf("Leggo %ld casuali dal files dal server.\n", nFilesToRead);

            FREE_N_ARGS(&nFilesToRead_string);
            break;
        case 'd':
            fprintf(stderr, "Errore, l'opzione -d va usata congiuntamente a -r o -R.\n");
            break;
        case 'l':
            TOKENIZER(selectedOption->arg, ",", printf("Lock su %s.\n", token));
            break;
        case 'u':
            TOKENIZER(selectedOption->arg, ",", printf("Unlock su %s.\n", token));
            break;
        case 'c':
            TOKENIZER(selectedOption->arg, ",", printf("Elimino file %s.\n", token));
            break;
        default:
            fprintf(stderr, "Errore opzione -%c gia' impostata.\n", selectedOption->opt); // opizioni -f -p o -t duplicate
            break;
        }

        nanosleep(&requestInterval, NULL);

        if (skipOption)
            skipOption = 0, selectedOption = selectedOption->next->next;
        else
            selectedOption = selectedOption->next;
    }

    free(sockname);
    freeOptionList(list);
    return 0;
}
