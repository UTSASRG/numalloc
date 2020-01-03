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
     insertListBeginning(list, newnode);
   }
   else {
     insertListAfter(list, list->last, newnode);
   }
}

// Whether a list is empty
inline bool isListEmpty(dlist* head) { return (head->next == head); }

// We donot check whether the list is empty or not?
inline dlist* tailList(dlist* head) {
  dlist* tail = NULL;
  if(!isListEmpty(head)) {
    tail = head->prev;
  }

  return tail;
}

// Insert one entry to two consequtive entries
inline void __insert_between(dlist* node, dlist* prev, dlist* next) {
  node->next = next;
  node->prev = prev;
  prev->next = node;
  next->prev = node;
}

// Insert one entry to after specified entry prev (prev, prev->next)
inline void listInsertNodeAfter(dlist* node, dlist* prev) { __insert_between(node, prev, prev->next); }
inline void listInsertNode(dlist* node, dlist* curr) { __insert_between(node, curr->prev, curr); }

// Insert one entry to the tail of specified list.
// Insert between tail and head
inline void listInsertTail(dlist* node, dlist* head) {
  __insert_between(node, head->prev, head);
}

inline void listUpdateEntry(dlist * node) {
  __insert_between(node, node->prev, node->next);
}

// Insert one entry to the head of specified list.
// Insert between head and head->next
inline void listInsertHead(dlist* node, dlist* head) { __insert_between(node, head, head->next); }

// Internal usage to link p with n
// Never use directly outside.
inline void __list_link(dlist* p, dlist* n) {
  p->next = n;
  n->prev = p;
}

// We need to verify this
// Insert one entry to the head of specified list.
// Insert the list between where and where->next
inline void listInsertList(dlist* list, dlist* where) {
  // Insert after where.
  __list_link(where, list);

  // Then modify other pointer
  __list_link(list->prev, where->next);
}

// Delete an entry and re-initialize it.
inline void listRemoveNode(dlist* node) {
  __list_link(node->prev, node->next);
  nodeInit(node);
}

// Delete an entry without re-initialization.
inline void listRemoveNodeOnly(dlist* node) {
  __list_link(node->prev, node->next);
}

// Check whether current node is the tail of a list
inline bool isListTail(dlist* node, dlist *list) { return (node->next = list); }

// Retrieve the first item form a list
// Then this item will be removed from the list.
inline dlist* listRetrieveItem(dlist* list) {
  dlist* first = NULL;

  // Retrieve item when the list is not empty
  if(!isListEmpty(list)) {
    first = list->next;
    listRemoveNode(first);
  }

  return first;
}

// Retrieve all items from a list and re-initialize all source list.
inline void listRetrieveAllItems(dlist* dest, dlist* src) {
  dlist* first, *last;
  first = src->next;
  last = src->prev;

  first->prev = dest;
  last->next = dest;
  dest->next = first;
  dest->prev = last;

  // reinitialize the source list
  listInit(src);
}

/* Get the pointer to the struct this entry is part of
 *
 */
#define listEntry(ptr, type, member) ((type*)((char*)(ptr) - (unsigned long)(&((type*)0)->member)))

#endif
