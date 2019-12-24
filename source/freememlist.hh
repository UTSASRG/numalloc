//
// Created by XIN ZHAO on 12/24/19.
//

#ifndef NUMALLOC_FREEMEMSINGLELIST_HH
#define NUMALLOC_FREEMEMSINGLELIST_HH

#include <new>

class FreeMemList;

class FreeMemNode {
private:
    volatile FreeMemNode *next;
    volatile FreeMemNode *pre;
    volatile size_t size;

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
        return const_cast<FreeMemNode *>(this->next);
    }

    FreeMemNode *getPre() {
        return const_cast<FreeMemNode *>(this->pre);
    }


};

class FreeMemList {
private:
    FreeMemNode head;
    FreeMemNode *tail = NULL;
    long length = 0;
private:
public:
    void insertIntoHead(FreeMemNode *node) {
        this->head.insertNext(node);
        length++;
        if (node->next == NULL) {
            tail = node;
        }
    }

    void insertIntoHead(void *ptr, size_t size) {
//        fprintf(stderr, "free node list insert head\n");
        FreeMemNode *node = FreeMemNode::create(ptr, size);
        this->insertIntoHead(node);
    }

    void remove(FreeMemNode *node) {
//        fprintf(stderr, "free node list remove\n");
        length--;
        if (this->tail == node) {
            this->tail = const_cast<FreeMemNode *>(node->pre);
        }
        if (this->tail == &this->head) {
            this->tail = NULL;
        }
        node->remove();
    }

    void replace(FreeMemNode *originNode, void *ptr, size_t size) {
//        fprintf(stderr, "free node list replace\n");
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
        if (this->tail == originNode) {
            this->tail = newNode;
        }
    }

    FreeMemNode *getHead() {
        return &head;
    }

    FreeMemNode *getTail() {
        return tail;
    }

    bool isEmpty() {
        if (head.next == NULL) {
            return true;
        }
        return false;
    }

    long getLength() {
        return length;
    }
};

#endif //NUMALLOC_FREEMEMSINGLELIST_HH
