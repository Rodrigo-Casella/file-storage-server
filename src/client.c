#include "../include/define_source.h"

#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../include/api.h"
#include "../include/cmdLineParser.h"
#include "../include/definitions.h"
#include "../include/io_utils.h"
#include "../include/utils.h"

#define HELP_MSG                                                                                                                        \
    "Uso: %s -f <socketfile> [OPTIONS]\n"                                                                                               \
    "-h,                    stampa questo messaggio e termina immediatamente.\n"                                                        \
    "-f <socketfile>,        specifica la socket a cui connettersi.\n"                                                                  \
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

#define CALL_API_ON_TOKEN(api, arg)                 \
    if (1)                                          \
    {                                               \
        char *savePtr;                              \
        char *token = strtok_r(arg, ",", &savePtr); \
                                                    \
        while (token)                               \
        {                                           \
            if (api(token) == -1)                   \
                perror(#api);                       \
            token = strtok_r(NULL, ",", &savePtr);  \
        }                                           \
    }

#define CHECK_SAVE_DIR(nextOption, d_or_D, saveDir) \
    if (nextOption && nextOption->opt == d_or_D)    \
    {                                               \
        skipOption = 1;                             \
        saveDir = nextOption->arg;                  \
    }

#define CHECK_IS_NUMBER(num_string, num)                                                                     \
    if (num_string && ((isNumber(num_string, &num) != 0)))                                                   \
    {                                                                                                        \
        fprintf(stderr, "Errore: %s, non e' un numero valido. Impostando valore di default.\n", num_string); \
        num = 0;                                                                                             \
    }

char *realpath(const char *path, char *resolved_path);

/**
 * @brief Effetua le chiamate alle api di openFile, writeFile, closeFile su file_path e sulla cartella di salvataggio save_dir (opzionale).
 *
 * @param file_path path del file su cui chiamare le api.
 * @param save_dir cartella di salvataggio opzionale.
 * @return 0 se successo, -1 altrimeni e errno settato.
 */
int writeFileHandler(const char *file_path, const char *save_dir)
{
    char resolved_path[PATH_MAX];

    if (!file_path)
    {
        errno = EINVAL;
        return -1;
    }

    CHECK_AND_ACTION(realpath, ==, NULL, perror("realpath"); fprintf(stderr, "Non e' stato possibile risolvere il percorso di %s\n", file_path); return -1, file_path, resolved_path);

    if (openFile(resolved_path, O_CREATE | O_LOCK) == -1)
    {
        perror("openFile");
        return -1;
    }

    if (writeFile(resolved_path, save_dir) == -1)
    {
        perror("writeFile");
        return -1;
    }

    if (closeFile(resolved_path) == -1)
    {
        perror("closeFile");
        return -1;
    }
    return 0;
}

/**
 * @brief Effetua le chiamate alle api di openFile, readFile, closeFile su file_path e salva il file letto nella cartella di salvataggio save_dir (opzionale).
 *
 * @param file_path path del file su cui chiamare le api.
 * @param save_dir cartella di salvataggio opzionale.
 * @return 0 se successo, -1 altrimeni e errno settato.
 */
int readFileHandler(const char *file_path, const char *save_dir)
{
    char resolved_path[PATH_MAX];

    char *buf;

    size_t size;

    if (!file_path)
    {
        errno = EINVAL;
        return -1;
    }

    CHECK_AND_ACTION(realpath, ==, NULL, perror("realpath"); fprintf(stderr, "Non e' stato possibile risolvere il percorso di %s\n", file_path); return -1, file_path, resolved_path);

    if (openFile(resolved_path, 0) == -1)
    {
        perror("openFile");
        return -1;
    }

    if (readFile(resolved_path, (void **)&buf, &size) == -1)
    {
        perror("readFile");
        return -1;
    }

    if (save_dir)
    {
        if (toPrint)
            printf("Salvando file su %s\n", save_dir);

        if (writeFileToDir(save_dir, file_path, buf, size) == -1)
            perror("writeFileToDir");
    }

    free(buf);

    if (closeFile(resolved_path) == -1)
    {
        perror("closeFile");
        return -1;
    }

    return 0;
}

int writeDirHandler(char *dirToWrite, const char *dirToSave, const int filesToWrite, int *filesWritten)
{
    DIR *dir;
    struct dirent *entry;
    struct stat info;
    char *currPath = NULL;

    SYSCALL_RET_EQ_ACTION(opendir, NULL, dir, return -1, dirToWrite);

    // ciclo finché trovo entry oppure ho raggiunto il limite superiore di files da scrivere
    while ((entry = readdir(dir)) && (filesToWrite <= 0 || *filesWritten < filesToWrite))
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        currPath = calloc(strlen(dirToWrite) + strlen(entry->d_name) + 2, sizeof(char));

        if (!currPath)
        {
            errno = ENOMEM;
            return -1;
        }

        strcpy(currPath, dirToWrite);
        strcat(currPath, "/");
        strcat(currPath, entry->d_name);

        if (stat(currPath, &info) == -1)
            perror("stat");

        if (S_ISDIR(info.st_mode))
        {
            if (writeDirHandler(currPath, dirToSave, filesToWrite, filesWritten) == -1)
            {
                closedir(dir);
                free(currPath);
                return -1;
            }
        }
        else
        {
            if (writeFileHandler(currPath, dirToSave) == -1)
            {
                closedir(dir);
                free(currPath);
                return 0;
            }

            (*filesWritten)++;
        }

        free(currPath);
    }

    SYSCALL_EQ_RETURN(closedir, -1, dir);

    return 0;
}

/**
 * @brief Chiama gli handler delle operazioni di scrittura o lettura su una stringa di file separati da un delimitatore e, se presente, su una cartella di salvataggio
 *
 * @param file_list stringa composta da i nomi dei file separati da un delimitatore
 * @param save_dir cartella di salvataggio opzionale
 * @param del demilimitatore che separa i nomi dei file
 * @param apiHandler handler dell'api da chiamare
 *
 * @return 0 se successo, -1 altrimenti e errno settato
 */
int callReadWriteOnList(char *file_list, char *save_dir, const char *delim, int (*apiHandler)(const char *, const char *))
{
    if (!file_list || !delim)
    {
        errno = EINVAL;
        return -1;
    }

    char *token,
        *savePtr;

    token = strtok_r(file_list, delim, &savePtr);

    while (token)
    {
        apiHandler(token, save_dir);
        token = strtok_r(NULL, delim, &savePtr);
    }

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

    CHECK_RET_AND_ACTION(getOption, ==, NULL, selectedOption, fprintf(stderr, "Errore, opzione -f necessaria.\n"); FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), list, 'f');

    CHECK_RET_AND_ACTION(strdup, ==, NULL, sockname, perror("strdup"); FREE_AND_EXIT(list, selectedOption, EXIT_FAILURE), selectedOption->arg);

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
    struct timespec abstime = {.tv_sec = time(NULL) + MAX_RETRY_TIME_SEC, .tv_nsec = 0};

    CHECK_AND_ACTION(openConnection, ==, -1, perror("openConnection"); free(sockname); FREE_AND_EXIT(list, NULL, EXIT_FAILURE), sockname, msec, abstime);

    selectedOption = list->head;
    int skipOption = 0;

    char *saveDir;

    while (selectedOption)
    {
        skipOption = 0;

        saveDir = NULL;

        switch (selectedOption->opt)
        {
        case 'w':;
            char *filesToWrite_string = NULL,
                *dirToWrite = NULL;

            long filesToWrite = 0;

            int filesWritten = 0;

            CHECK_SAVE_DIR(selectedOption->next, 'D', saveDir);

            getNTokens(selectedOption->arg, ",", 2, &dirToWrite, &filesToWrite_string);

            CHECK_IS_NUMBER(filesToWrite_string, filesToWrite);

            if (writeDirHandler(dirToWrite, saveDir, filesToWrite, &filesWritten) || filesWritten <= 0)
                fprintf(stderr, "Non è stato possibile scrivere i file della directory %s sul server.\n", dirToWrite);
            break;
        case 'W':;
            CHECK_SAVE_DIR(selectedOption->next, 'D', saveDir);

            if (callReadWriteOnList(selectedOption->arg, saveDir, ",", writeFileHandler) == -1)
                perror("callReadWriteOnList");
            break;
        case 'D':
            fprintf(stderr, "Errore, l'opzione -D va usata congiuntamente a -w o -W.\n");
            break;
        case 'r':;
            CHECK_SAVE_DIR(selectedOption->next, 'd', saveDir);

            if (callReadWriteOnList(selectedOption->arg, saveDir, ",", readFileHandler) == -1)
                perror("callReadWriteOnList");
            break;
        case 'R':;
            long files_to_read = 0;
            CHECK_SAVE_DIR(selectedOption->next, 'd', saveDir);

            CHECK_IS_NUMBER(selectedOption->arg, files_to_read);

            if (readNFiles(files_to_read, saveDir) == -1)
                perror("readNFiles");
            break;
        case 'd':
            fprintf(stderr, "Errore, l'opzione -d va usata congiuntamente a -r o -R.\n");
            break;
        case 'l':
            CALL_API_ON_TOKEN(lockFile, selectedOption->arg);
            break;
        case 'u':
            CALL_API_ON_TOKEN(unlockFile, selectedOption->arg);
            break;
        case 'c':
            CALL_API_ON_TOKEN(removeFile, selectedOption->arg);
            break;
        default:
            fprintf(stderr, "Errore opzione -%c gia' impostata.\n", selectedOption->opt); // opizioni -f -p o -t duplicate
            break;
        }

        nanosleep(&requestInterval, NULL);

        selectedOption = selectedOption->next;

        if (skipOption && selectedOption)
            selectedOption = selectedOption->next;
    }

    CHECK_AND_ACTION(closeConnection, ==, -1, perror("closeConnection"), sockname);
    free(sockname);
    freeOptionList(list);
    return 0;
}
