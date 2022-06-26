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

Option *getOption(OptionList *list, char opt);
OptionList *parseCmdLine(int argc, char *argv[]);
void printOption(Option *option);
void printOptionList(OptionList *list);
void freeOption(Option *option);
void freeOptionList(OptionList *list);

#endif