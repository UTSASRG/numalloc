/*
 * FreeGuard: A Faster Secure Heap Allocator
 * Copyright (C) 2017 Sam Silvestro, Hongyu Liu, Corey Crosser, 
 *                    Zhiqiang Lin, and Tongping Liu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * 
 * @file   dlist.h: a doubly linked circularly linked list.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 */
#ifndef __DLIST_H__
#define __DLIST_H__

#include <stdio.h>

typedef struct dlist {
  struct dlist* next;
  struct dlist* prev;
} dlist_t;

// This is a special list, where the first node's prev is always pointing to the list header. 
// That is, every entry of the list's prev will never be NULL
// The last node's next will point to NULL so that we could check the last node of the list. 
// The header's next will always point to the first entry, where its last will point to the last entry
// When the list is empty, both prev and next pointer will be NULL in the end.
inline void initDLList(dlist_t* list) { list->next = list->prev = NULL; }

inline void initDLEntry(dlist_t * node) {
  node->next = NULL;
  node->prev = NULL;
}

// Insert a node to the end of a list 
inline void insertDLEnd(dlist_t * list, dlist_t * node) {
  dlist_t * lastNode = list->prev;

  // Check whether the list is empty before
  if(lastNode == NULL) {
    // the next node is the current node
    list->next = node;
    node->prev = list;
  }
  else {
    lastNode->next = node;
    node->prev = lastNode;
  }
    
  node->next = NULL;
  list->prev = node;
}

inline void insertDLBegin(dlist_t * list, dlist_t * node) {
  dlist_t * firstNode = list->next; 

  // Check whether the list is empty before
  if(firstNode == NULL) {
    // For the first node, both the last and the first node is the current node
    // Since there is no predecessor, the node's prev and next node will be empty for the first node
    list->prev = node;
    node->next = NULL;
  }
  else {
    // Insert in the beginning
    firstNode->prev = node;
    node->next = firstNode;
  }
  node->prev = list;
  list->next = node;
}

// Delete an entry and re-initialize it.
inline void removeDLNode(dlist_t* list, dlist_t *node) {
  node->prev->next = node->next;

  if(node->next == NULL) {
    // There is no next entry for the list. 
    list->next = NULL;
  }
  else {
    node->next->prev = node->prev;
  } 
	
	return;
}

#endif
