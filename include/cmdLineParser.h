#ifndef CMDLINEPARSER_H
#define CMDLINEPARSER_H

typedef struct option
{
    char opt;
    char *arg;
    struct option *next;
} Option;

typedef struct optionList
{
    Option *head;
    Option *tail;
} OptionList;

OptionList *parseCmdLine(int argc, char *argv[]);
void printOptionList(OptionList *list);
void freeOptionList(OptionList *list);

#endif