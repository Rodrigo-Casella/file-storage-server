#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

typedef struct setting
{
    char *key;
    char *value;
    struct setting *next;
} Setting;

Setting *parseFile(const char *path);
char *getValue(Setting *settings, const char *key);
long getNumericValue(Setting *settings, const char *key);
void freeSettingList(Setting **head);
#endif