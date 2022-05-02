#include "../include/define_source.h"

#include <stdio.h>
#include <stdlib.h>

#include "../include/configParser.h"
#include "../include/utils.h"

static int readAndAddSetting(char *line, Setting **head)
{
    Setting *newSetting = NULL;
    CHECK_RET_AND_ACTION(malloc, ==, NULL, newSetting, perror("malloc newSetting"); return -1, sizeof(*newSetting));

    char *token = NULL, *save_ptr = NULL;
    int token_length = 0;

    CHECK_RET_AND_ACTION(strtok_r, ==, NULL, token, fprintf(stderr, "Errore leggendo chiave impostazione\n"); return -1, line, "=", &save_ptr);
    token_length = strlen(token) + 1;

    CHECK_RET_AND_ACTION(malloc, ==, NULL, newSetting->key, perror("malloc chiave"), sizeof(char) * token_length);
    strncpy(newSetting->key, token, token_length);

    CHECK_RET_AND_ACTION(strtok_r, ==, NULL, token, fprintf(stderr, "Errore leggendo valore impostazione\n"); return -1, NULL, "=", &save_ptr);
    token_length = strlen(token) + 1;

    CHECK_RET_AND_ACTION(malloc, ==, NULL, newSetting->value, perror("malloc valore"); return -1, sizeof(char) * token_length);
    strncpy(newSetting->value, token, token_length);
    newSetting->value[token_length] = '\0';
    
    newSetting->next = *head;
    *head = newSetting;

    return 0;
}

Setting *parseFile(const char *path)
{
    Setting *settings = NULL;

    FILE *fp = NULL;

    CHECK_RET_AND_ACTION(fopen, ==, NULL, fp, perror("fopen"); return NULL, path, "r");

    char buf[BUF_SIZE];

    while (fgets(buf, BUF_SIZE, fp))
    {
        if (strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0)
            continue;

        readAndAddSetting(buf, &settings);
    }
    if (feof(fp) == 0)
    {
        fprintf(stderr, "Errore fgets\n");
        return NULL;
    }

    CHECK_AND_ACTION(fclose, ==, -1, perror("fclose"); return NULL, fp);

    return settings;
}

char *getValue(Setting *settings, const char *key)
{
    Setting *curr = settings;

    while (curr)
    {
        if (strncmp(key, curr->key, strlen(key) + 1) == 0)
        {
            char *value = strndup (curr->value, strlen(curr->value) + 1);
            return value;
        }
        curr = curr->next;
    }

    return NULL;
}

long getNumericValue(Setting *settings, const char *key)
{
    char *value = getValue(settings, key);

    if (!value)
        return -1;

    long nValue = -1;

    if (isNumber(value, &nValue) != 0)
    {
        perror("isNumber");
    }

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