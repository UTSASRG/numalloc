//
// Created by XIN ZHAO on 12/21/19.
//

#ifndef NUMALLOC_FREEMEMLIST_HH
#define NUMALLOC_FREEMEMLIST_HH

#include "dlist.hh"
#include <new>

class FreeMemList;

class FreeMemNode {
private:
    FreeMemNode *next;
    FreeMemNode *pre;
    size_t size;

    friend class FreeMemList;

private:
    FreeMemNode() {
        this->next = NULL;
        this->pre = NULL;
        this->size = 0;
    }

    FreeMemNode(size_t size) {
        this->next = NULL;
        this->pre = NULL;
        this->size = size;
    }

    // insert exact behind this node
    void insertNext(FreeMemNode *freeMemNode) {
        freeMemNode->pre = this;
        freeMemNode->next = this->next;
        if (NULL != this->next) {
            this->next->pre = freeMemNode;
        }
        this->next = freeMemNode;
    }

    void remove() {
        this->pre->next = this->next;
        if (NULL != this->next) {
            this->next->pre = this->pre;
        }
    }

public:
    static FreeMemNode *create(void *ptr, size_t size) {
        assert(size >= sizeof(FreeMemNode));
        FreeMemNode *freeMemNode = new(ptr)FreeMemNode(size);
        return freeMemNode;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    size_t getSize() {
        return size;
    }

    FreeMemNode *getNext() {
        return this->next;
    }

    FreeMemNode *getPre() {
        return this->pre;
    }


};

class FreeMemList {
private:
    FreeMemNode head;
private:
public:
    void insertIntoHead(FreeMemNode *node) {
        this->head.insertNext(node);
    }

    void insertIntoHead(void *ptr, size_t size) {
        fprintf(stderr, "free node list insert\n");
        FreeMemNode *node = FreeMemNode::create(ptr, size);
        this->insertIntoHead(node);
    }

    void remove(FreeMemNode *node) {
        fprintf(stderr, "free node list remove\n");
        node->remove();
    }

    void replace(FreeMemNode *originNode, void *ptr, size_t size) {
        FreeMemNode *newNode = FreeMemNode::create(ptr, size);
        this->replace(originNode, newNode);
    }

    void replace(FreeMemNode *originNode, FreeMemNode *newNode) {
        originNode->pre->next = newNode;
        newNode->pre = originNode->pre;
        newNode->next = originNode->next;
        if (NULL != originNode->next) {
            originNode->next->pre = newNode;
        }
    }

    FreeMemNode *getHead() {
        return &head;
    }

    bool isEmpty() {
        if (head.next == NULL) {
            return true;
        }
        return false;
    }
};


#endif //NUMALLOC_FREEMEMLIST_HH
