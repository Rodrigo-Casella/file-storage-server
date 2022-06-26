#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/cmdLineParser.h"

extern char *optarg;
extern int optopt, optind;

#define CHECK_OPTIONAL_ARGUMENT                             \
    if (!optarg && optind < argc && argv[optind][0] != '-') \
    {                                                       \
        optarg = argv[optind++];                            \
    }

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
        newOption->arg = calloc(argLength, sizeof(char));

        if (!newOption->arg)
        {
            perror("allocOption newOption->arg");
            return NULL;
        }

        strcpy(newOption->arg, arg);
        newOption->arg[argLength - 1] = '\0';
        newOption->arg[strcspn(newOption->arg, "\n")] = '\0';
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

Option *getOption(OptionList *list, char opt)
{
    if (!list)
    {
        fprintf(stderr, "getOption: lista vuota");
        return NULL;
    }

    Option *curr = list->head, *prev = NULL, *option = NULL;

    while (curr)
    {
        if (curr->opt == opt)
        {
            option = curr;

            if (!prev)
            {
                list->head = curr->next;
                break;
            }

            if (curr == list->tail)
            {
                list->tail = prev;
                prev->next = NULL;
                break;
            }

            prev->next = curr->next;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    return option;
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

    for (int opt = 0; (opt = getopt(argc, argv, ":hf:w:W:D:r:R::d:t:l:u:c:p")) != -1;)
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

            addOption(list, opt, !optarg ? "0" : optarg);
            break;
        default:
            if (CHECK_ARGUMENT)
            {
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

void printOption(Option *option)
{
    if (option)
        printf("Opt -%c ---> %s\n", option->opt, (!option->arg ? "NULL" : option->arg));
}

void printOptionList(OptionList *list)
{
    Option *tmp = list->head;

    while (tmp)
    {
        printOption(tmp);
        tmp = tmp->next;
    }
}

void freeOption(Option *option)
{
    if (option)
    {
        if (option->arg)
            free(option->arg);

        free(option);
    }
}

void freeOptionList(OptionList *list)
{
    Option *tmp = list->head;

    while (tmp)
    {
        list->head = list->head->next;
        freeOption(tmp);
        tmp = list->head;
    }

    free(list);
}