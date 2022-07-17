#include "../include/define_source.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../include/io_utils.h"
#include "../include/utils.h"

/**
 * @brief Crea ricorsivamente cartelle
 *
 * @note Adattato da https://nachtimwald.com/2019/07/10/recursive-create-directory-in-c-revisited/
 *
 * @param dir pathname completo da creare
 * @return 0 se successo, -1 altrimenti e ernno settato
 */
static int recursive_mkdir(const char *dir)
{
    char *tmp;
    const char *dirPtr;
    size_t len;

    if (!dir)
    {
        errno = EINVAL;
        return -1;
    }

    len = strlen(dir) + 1;

    tmp = calloc(len, sizeof(char));

    if (!tmp)
    {
        errno = ENOMEM;
        return -1;
    }

    dirPtr = dir;

    while ((errno = 0, dirPtr = strchr(dirPtr, '/')) != NULL)
    {
        if (dirPtr == dir && *dirPtr == '/')
        {
            dirPtr++;
            continue;
        }

        memcpy(tmp, dir, dirPtr - dir);
        tmp[dirPtr - dir] = '\0';
        dirPtr++;

        if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST)
            break;
    }

    free(tmp);

    mkdir(dir, S_IRWXU);

    errno = errno != EEXIST ? errno : 0;

    return errno ? -1 : 0;
}

int writeFileToDisk(const char *path, void *buf, size_t size)
{
    char *lastDir;

    int file_fd;

    errno = 0;
    if (!path || !buf || size < 1)
    {
        errno = EINVAL;
        return -1;
    }

    lastDir = strrchr(path, '/'); // path meno le cartelle fino al filename

    if (lastDir)
    {
        *lastDir = '\0'; // escludo il filename dal path per creare ricorsivamente le cartelle di cui ho bisogno
        if (recursive_mkdir(path) == -1)
            return -1;

        *lastDir = '/';
    }

    if ((file_fd = open(path, O_WRONLY | O_CREAT, S_IRWXU)) == -1)
        return -1;

    if (writen(file_fd, buf, size) == -1)
    {
        SAVE_ERRNO_AND_RETURN(close(file_fd), -1);
    }

    close(file_fd);

    return errno ? -1 : 0;
}

int writeFileToDir(const char* save_dir, const char *file_path, void *file_data, size_t file_size)
{
    char *save_dir_path_buf;

    int retval;

    if (!save_dir || strlen(save_dir) <= 0 || !file_path || strlen(file_path) <= 0 || !file_data || file_size <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    save_dir_path_buf = calloc(strlen(save_dir) + strlen(file_path) + 2, sizeof(char));

    if (!save_dir_path_buf)
    {
        errno = ENOMEM;
        return -1;
    }

    strcat(save_dir_path_buf, save_dir);
    strcat(save_dir_path_buf, file_path);

    if ((retval = writeFileToDisk(save_dir_path_buf, file_data, file_size)) == -1)
        fprintf(stderr, "Non è stato possibile scrivere il file %s\n", file_path);

    free(save_dir_path_buf);

    return retval;
}

char *readFileFromPath(const char *path, size_t *file_len)
{
    char *file_data;

    FILE *fp;

    errno = 0;

    if ((fp = fopen(path, "r")) == NULL)
        return NULL;

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        SAVE_ERRNO_AND_RETURN(fclose(fp), NULL);
    }
        
    if ((*file_len = ftell(fp)) < 0)
    {
        SAVE_ERRNO_AND_RETURN(fclose(fp), NULL);
    }

    rewind(fp);

    file_data = calloc((*file_len), sizeof(char));

    if (!file_data)
    {
        errno = ENOMEM;
        SAVE_ERRNO_AND_RETURN(fclose(fp), NULL);
    }

    if (fread(file_data, sizeof(char), *file_len, fp) <= 0)
    {
        if (ferror(fp))
        {
            SAVE_ERRNO_AND_RETURN(fclose(fp); free(file_data), NULL);
        }
    }

    if (fclose(fp) == -1)
        free(file_data);

    return errno ? NULL : file_data;
}

char *readFileFromServer(int fd_skt, size_t *file_len)
{
    char *file_data;

    errno = 0;

    if (readn(fd_skt, file_len, sizeof(size_t)) == -1)
        return NULL;

    if (*file_len == 0)
        return NULL;

    file_data = calloc(*file_len, sizeof(char));

    if (!file_data)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (readn(fd_skt, file_data, *file_len) == -1)
        free(file_data);

    return errno ? NULL : file_data;
}

int readMultipleFilesFromServer(int fd_skt, const char *save_dir)
{
    int files_read;
        
    char *file_path,
        *file_data;

    size_t file_path_len,
        file_len;

    files_read = 0;
    while (1)
    {
        file_data = NULL;

        file_path_len = file_len = 0;

        file_path = readFileFromServer(fd_skt, &file_path_len);

        if (file_path_len == 0) // Non ho più file da leggere
            return files_read;

        if (!file_path)
            return -1;

        file_data = readFileFromServer(fd_skt, &file_len);

        if (save_dir && file_data)
            writeFileToDir(save_dir, file_path, file_data, file_len);

        if (file_path)
            free(file_path);
        if (file_data)
            free(file_data);

        
        files_read++;
    }

    return files_read;
}