#ifndef FDLIST_H
#define FDLIST_H

typedef struct fd_node
{
    struct fd_node *prev;
    int fd;
    struct fd_node *next;
} fdNode;

typedef struct fd_list
{
    fdNode *head;
    fdNode *tail;
} fdList;

fdNode *initNode(int fd);
void deleteNode(fdNode *node);
fdList *initList();
void deleteList(fdList **list);
fdNode *getNode(fdList *list, int key);
fdNode *popNode(fdList *list);
/**
 * @brief Cerca il nodo con la chive 'key' nella lista
 *
 * @param list lista in cui cercare
 * @param key chiave del nodo
 * \retval 1 se ho trovato il nodo
 * \retval 0 se non ho trovato il nodo
 */
int findNode(fdList *list, int key);
int insertNode(fdList *list, int fd);

#endif