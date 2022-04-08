#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/cmdLineParser.h"

extern char *optarg;
extern int optopt, optind;

#define CHECK_OPTIONAL_ARGUMENT \
 if (!optarg && optind < argc && argv[optind][0] != '-') \
 { \
    optarg = argv[optind++]; \
 } \

#define CHECK_ARGUMENT (optarg && optarg[0] == '-')

static Option *allocOption(char opt, char *arg)
{
    Option *newOption = malloc(sizeof(*newOption));

    if (!newOption)
    {
        perror("newOption");
        return NULL;
    }

    newOption->opt = opt;
    newOption->arg = NULL;
    if (arg)
    {
        size_t argLength = strlen(arg) + 1;
        newOption->arg = malloc(sizeof(char) * argLength);

        if (!newOption->arg)
        {
            perror("allocOption newOption->arg");
            return NULL;
        }

        strncpy(newOption->arg, arg, argLength);
    }

    newOption->next = NULL;
    return newOption;
}

static void addOption(OptionList *list, char opt, char *arg)
{
    if (!list)
    {
        fprintf(stderr, "addOption: list e' NULL\n");
        return;
    }

    Option *newOption = allocOption(opt, arg);

    if (!newOption)
    {
        fprintf(stderr, "Non è stato possibile allocare una nuova opzione");
        return;
    }

    if (!list->head)
    {
        list->head = list->tail = newOption;
    }
    else
    {

        list->tail->next = newOption;
        list->tail = list->tail->next;
    }
}

static OptionList *initList()
{
    OptionList *newList = malloc(sizeof(*newList));

    if (!newList)
    {
        perror("newList");
        return NULL;
    }

    newList->head = NULL;
    newList->tail = NULL;

    return newList;
}

OptionList *parseCmdLine(int argc, char *argv[])
{
    OptionList *list = initList();

    if (!list)
    {
        fprintf(stderr, "Non è stato possibile inizializzare una nuova lista di opzioni");
        return NULL;
    }

    for (int opt = 0; (opt = getopt(argc, argv, ":hf:w:W:D:r:R::d:t:l:c:u:p")) != -1;)
    {
        switch (opt)
        {
        case '?':
            fprintf(stderr, "-%c non è riconosciuta come opzione\n", optopt);
            break;
        case ':':
            fprintf(stderr, "-%c necessita di un argomento\n", optopt);
            break;

        case 'R':
            CHECK_OPTIONAL_ARGUMENT; 
            if (!optarg) {
                addOption(list, opt, "n=0");
                break;
            }
            addOption(list, opt, optarg);
            break;
        default:
            if (CHECK_ARGUMENT) {
                fprintf(stderr, "-%c argomento non valido\n", opt);
                optind--;
                break;
            }
            addOption(list, opt, optarg);
            break;
        }
    }

    return list;
}

void printOptionList(OptionList *list)
{
    Option *tmp = list->head;

    while (tmp)
    {
        printf("Opt -%c ---> %s\n", tmp->opt, (!tmp->arg ? "NULL" : tmp->arg));
        tmp = tmp->next;
    }
}

void freeOptionList(OptionList *list)
{
    Option *tmp = list->head;

    while (tmp)
    {
        list->head = list->head->next;
        if (tmp->arg)
            free(tmp->arg);
        free(tmp);
        tmp = list->head;
    }

    free(list);
}