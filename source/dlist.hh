#if !defined(_LIST_H)
#define _LIST_H

#include <stdlib.h>

typedef struct dlist_t {
  struct dlist_t * prev;
  struct dlist_t * next;
} dlist;

typedef struct dlistHead_t {
  struct dlist_t * first;
  struct dlist_t * last;
} dlistHead;

inline void listInit(dlistHead * list) { list->first = NULL; list->last = NULL; }

inline void listInsertAfter(dlistHead * list, dlist * node, dlist * newnode) {
  newnode->prev = node;
  if(node->next == NULL) {
    newnode->next = NULL;
    list->last = newnode;
  }
  else {
    newnode->next = node->next;
    node->next->prev = newnode;
  }
  node->next = newnode;
}

inline void listInsertBefore(dlistHead * list, dlist * node, dlist * newnode) {
  newnode->next = node;
  if(node->prev == NULL) {
    newnode->prev = NULL;
    list->first = newnode;
  }
  else {
    newnode->prev = node->prev;
    node->prev->next = newnode;
  }
  node->prev = newnode;
}

inline void insertListBeginning(dlistHead * list, dlist * newnode) {
  if (list->first == NULL) {
    list->first  = newnode;
    list->last   = newnode;
    newnode->prev = NULL;
    newnode->next  = NULL;
  }
  else {
    listInsertBefore(list, list->first, newnode);
  }
}

inline void insertListEnd(dlistHead * list, dlist * newnode) {
   if(list->last == NULL) {
     // No existing node
     insertListBeginning(list, newnode);
   }
   else {
     listInsertAfter(list, list->last, newnode);
   }
}

inline void listRemoveNode (dlistHead * list, dlist * node) {
    if (node->prev == NULL) {
      // First node 
      list->first  = node->next;
    }
    else {
      node->prev->next  = node->next;
    }

    if (node->next == NULL) {
      // Last node 
      list->last  = node->prev;
    }
    else {
      node->next->prev  = node->prev;
    }
}

#endif
