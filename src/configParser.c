#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>

#include "../include/configParser.h"
#include "../include/utils.h"

static int readAndAddSetting(char *setting, Setting **head)
{
    Setting *newSetting;

    char *token;

    CHECK_RET_AND_ACTION(malloc, ==, NULL, newSetting, errno = ENOMEM; return -1, sizeof(*newSetting));

    CHECK_RET_AND_ACTION(strtok, ==, NULL, token, errno = ENODATA; return -1, setting, "=");
    CHECK_RET_AND_ACTION(strdup, ==, NULL, newSetting->key, return -1, token);

    CHECK_RET_AND_ACTION(strtok, ==, NULL, token, errno = ENODATA; return -1, NULL, "=");
    CHECK_RET_AND_ACTION(strdup, ==, NULL, newSetting->value, return -1, token);
    
    newSetting->next = *head;
    *head = newSetting;

    return 0;
}

Setting *parseFile(const char *path)
{
    Setting *settings = NULL;

    FILE *fp = NULL;

    CHECK_RET_AND_ACTION(fopen, ==, NULL, fp, return NULL, path, "r");

    char buf[BUF_SIZE];

    while (fgets(buf, BUF_SIZE, fp))
    {
        if (strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0)
            continue;

        buf[strcspn(buf, "\n")] = '\0';
        if (readAndAddSetting(buf, &settings) == -1)
        {
            SAVE_ERRNO_AND_RETURN(freeSettingList(&settings); fclose(fp), NULL);
        }
    }
    if (feof(fp) == 0)
    {
        SAVE_ERRNO_AND_RETURN(freeSettingList(&settings); fclose(fp), NULL);
    }

    CHECK_AND_ACTION(fclose, ==, -1, return NULL, fp);

    return settings;
}

char *getValue(Setting *settings, const char *key)
{
    Setting *curr = settings;

    while (curr)
    {
        if (strncmp(key, curr->key, strlen(key)) == 0)
        {
            char *value = strdup (curr->value);
            return value;
        }
        curr = curr->next;
    }
    errno = ENOENT;
    return NULL;
}

long getNumericValue(Setting *settings, const char *key)
{
    char *value = getValue(settings, key);

    if (!value)
        return -1;

    long nValue = -1;

    if (isNumber(value, &nValue) != 0)
        errno = EINVAL;

    free(value);
    return nValue;
}

void freeSettingList(Setting **head)
{
    Setting *tmp;

    while (*head)
    {
        tmp = *head;
        *head = (*head)->next;

        if (tmp->key)
            free(tmp->key);
        if (tmp->value)
            free(tmp->value);
        free(tmp);
    }
}