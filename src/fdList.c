#include "../include/define_source.h"

#include <stdlib.h>

#include "../include/fdList.h"
#include "../include/utils.h"

fdNode *initNode(int fd)
{   
    fdNode *newNode;

    if (fd <= 0)
    {
        errno = EINVAL;
        return NULL;
    }
    
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newNode, errno = ENOMEM; return NULL, 1, sizeof(*newNode));

    newNode->fd = fd;
    newNode->next = newNode->prev = NULL;

    return newNode;
}

void deleteNode(fdNode *node)
{
    if (node)
        free(node);
}

fdList *initList()
{
    fdList *newList;
    CHECK_RET_AND_ACTION(calloc, ==, NULL, newList, errno = ENOMEM; return NULL, 1, sizeof(*newList));

    newList->head = newList->tail = NULL;

    return newList;
}

void deleteList(fdList **list)
{
    fdNode *tmp;

    if (!(*list))
        return;

    while ((*list)->head)
    {
        tmp = (*list)->head;
        (*list)->head = (*list)->head->next;
        deleteNode(tmp);
    }

    free(*list);

    *list = NULL;
}

fdNode *getNode(fdList *list, int key)
{
    if (!list || key <= 0)
    {
        errno = EINVAL;
        return NULL;
    }

    fdNode *curr = list->head;

    while (curr)
    {
        if (curr->fd == key)
        {
            if (!curr->prev)
            {
                list->head = curr->next;
            }
            else
            {
                curr->prev->next = curr->next;
                curr->next->prev = curr->prev;
            }

            curr->next = curr->prev = NULL;
            return curr;
        }
    }
    return NULL;
}

fdNode *popNode(fdList *list)
{
    if (!list)
        return NULL;
    
    fdNode *ret = list->head;

    if (list->head)
        list->head = list->head->next;

    if (ret && ret->next)
        ret->next->prev = ret->prev;

    return !ret ? NULL : ret;
}

int findNode(fdList *list, int key)
{
    if (!list || key <= 0)
    {
        errno = EINVAL;
        return 0;
    }

    fdNode *curr = list->head;

    while (curr)
    {
        if (curr->fd == key)
            return 1;

        curr = curr->next;
    }

    return 0;
}

int insertNode(fdList *list, int fd)
{
    if (!list || fd <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    fdNode *newNode = initNode(fd);

    if (!newNode)
        return -1;

    if (!list->head)
    {
        list->head = newNode;
    }
    else
    {
        newNode->prev = list->tail;
        if (list->tail)
            list->tail->next = newNode;
    }

    list->tail = newNode;
    return 0;
}