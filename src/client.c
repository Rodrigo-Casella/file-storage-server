#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500

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
#include "../include/api.h"

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

#define RETRY_TIME_MSEC 1000
#define MAX_RETRY_TIME_SEC 5
#define FREE_AND_EXIT(list, option, status) \
    freeOption(option);                     \
    freeOptionList(list);                   \
    exit(status);

#define PRINT_HELP_MSG_AND_EXIT(list, option) \
    printf(HELP_MSG, argv[0]);                \
    FREE_AND_EXIT(list, option, EXIT_SUCCESS);

#define INVALID_ARGUMENT(option, arg) fprintf(stderr, "Errore opzione -%c: argomento '%s' non valido.\n", option, arg)

#define CHECK_SAVE_DIR(nextOption, d_or_D, saveDir)                         \
    if (nextOption && nextOption->opt == d_or_D)                            \
        skipOption = 1;                                                     \
                                                                            \
    if (skipOption)                                                         \
        CHECK_RET_AND_ACTION(strndup, ==, NULL, saveDir, perror("strndup"), \
                             nextOption->arg, strlen(nextOption->arg) + 1);

#define CHECK_IS_NUMBER(num_string, num)                                                                           \
    if (num_string && ((isNumber(num_string + 2, &num) != 0) || num < 0)) /* nFiles + 2: salto i caratteri n=*/    \
    {                                                                                                              \
        fprintf(stderr, "Errore: %s non e' un numero o e' negativo. Impostando valore di default.\n", num_string); \
        num = 0;                                                                                                   \
    }

int writeFileHandler(char *file_path)
{
    char resolved_path[PATH_MAX];
    SYSCALL_EQ_ACTION(realpath, NULL, fprintf(stderr, "Non e' stato possibile risolvere il percorso di %s\n", file_path); return -1, file_path, resolved_path);
    printf("scrivendo: %s\n", resolved_path);
    return 0;
}

int readFileHandler(char *file)
{
    printf("leggendo: %s\n", file);
    return 0;
}

int writeDirHandler(char const *dirToWrite, char const *dirToSave, long const filesToWrite, int *filesWritten)
{
    DIR *dir;
    struct dirent *entry;

    char cwd[PATH_MAX];
    SYSCALL_EQ_RETURN(getcwd, NULL, cwd, PATH_MAX);

    SYSCALL_RET_EQ_ACTION(opendir, NULL, dir, fprintf(stderr, "Errore aprendo directory: %s.\n", dirToWrite); return -1, dirToWrite);

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
            writeDirHandler(entry->d_name, dirToSave, filesToWrite, filesWritten);
        }
        else
        {
            CHECK_AND_ACTION(writeFileHandler, ==, -1, return -1, entry->d_name);
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

    SYSCALL_EQ_RETURN(chdir, -1, cwd);
    return 0;
}

void callApiOnToken(char *string, char *const del, int (*apiHandler)(char *))
{
    char *token, *savePtr;

    token = strtok_r(string, del, &savePtr);

    while (token != NULL)
    {
        apiHandler(token);
        token = strtok_r(NULL, del, &savePtr);
    }
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

    CHECK_RET_AND_ACTION(getOption, ==, NULL, selectedOption, fprintf(stderr, "Errore, opzione -f necessaria.\n"); FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), list, 'f');

    CHECK_RET_AND_ACTION(strndup, ==, NULL, sockname, FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), selectedOption->arg, strlen(selectedOption->arg) + 1);

    freeOption(selectedOption);

    struct timespec requestInterval;
    long requestIntervalMsec = 0; // valore di default

    if ((selectedOption = getOption(list, 't')))
    {
        if (isNumber(selectedOption->arg, &requestIntervalMsec))
            INVALID_ARGUMENT(selectedOption->opt, selectedOption->arg);

        freeOption(selectedOption);
    }
    setTimespecMsec(&requestInterval, requestIntervalMsec);

    if ((selectedOption = getOption(list, 'p')))
    {
        puts("Prints enables");
        toPrint = 1;
        freeOption(selectedOption);
    }

    int msec = RETRY_TIME_MSEC;
    struct timespec abstime = { .tv_sec = time(NULL) + MAX_RETRY_TIME_SEC, .tv_nsec = 0};

    printf("Connessione a %s in corso\n", sockname);
    CHECK_AND_ACTION(openConnection, ==, -1, perror("openConnection");free(sockname);FREE_AND_EXIT(list, NULL, EXIT_FAILURE), sockname, msec, abstime);

    selectedOption = list->head;
    int skipOption = 0;

    while (selectedOption)
    {
        char *nFiles_string = NULL, *saveDir = NULL;
        long nFiles = 0;

        switch (selectedOption->opt)
        {
        case 'w':;
            CHECK_SAVE_DIR(selectedOption->next, 'D', saveDir);

            char *dirToWrite = NULL;

            dupNTokens(selectedOption->arg, ",", 2, &dirToWrite, &nFiles_string);

            CHECK_IS_NUMBER(nFiles_string, nFiles);

            int filesWritten = 0;

            if (writeDirHandler(dirToWrite, saveDir, nFiles, &filesWritten) || filesWritten <= 0)
                fprintf(stderr, "Non è stato possibile scrivere i file della directory %s sul server.\n", dirToWrite);

            freeNargs(3, &saveDir, &dirToWrite, &nFiles_string);
            break;
        case 'W':;
            CHECK_SAVE_DIR(selectedOption->next, 'D', saveDir);

            callApiOnToken(selectedOption->arg, ",", writeFileHandler);

            if (saveDir)
                free(saveDir);
            break;
        case 'D':
            fprintf(stderr, "Errore, l'opzione -D va usata congiuntamente a -w o -W.\n");
            break;
        case 'r':;
            CHECK_SAVE_DIR(selectedOption->next, 'd', saveDir);

            callApiOnToken(selectedOption->arg, ",", readFileHandler);

            if (saveDir)
                free(saveDir);
            break;
        case 'R':;
            CHECK_SAVE_DIR(selectedOption->next, 'd', saveDir);

            CHECK_RET_AND_ACTION(strndup, ==, NULL, nFiles_string, perror("strndup"), selectedOption->arg, strlen(selectedOption->arg) + 1);

            CHECK_IS_NUMBER(nFiles_string, nFiles);

            printf("Leggo %ld casuali dal files dal server.\n", nFiles);

            freeNargs(2, &saveDir, &nFiles_string);
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
