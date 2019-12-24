//
// Created by XIN ZHAO on 12/21/19.
//

#ifndef NUMALLOC_FREEMEMSINGLELIST_HH
#define NUMALLOC_FREEMEMSINGLELIST_HH

#include <new>

class FreeMemSingleList;

class FreeMemSingleNode {
private:
    volatile FreeMemSingleNode *next;
    volatile size_t size;

    friend class FreeMemSingleList;

private:
    FreeMemSingleNode() {
        this->next = NULL;
        this->size = 0;
    }

    FreeMemSingleNode(size_t size) {
        this->next = NULL;
        this->size = size;
    }

    // insert exact behind this node
    void insertNext(FreeMemSingleNode *freeMemNode) {
        freeMemNode->next = this->next;
        this->next = freeMemNode;
    }

    void remove(FreeMemSingleNode *preNode) {
        preNode->next = this->next;
    }

public:
    static FreeMemSingleNode *create(void *ptr, size_t size) {
        assert(size >= sizeof(FreeMemSingleNode));
        FreeMemSingleNode *freeMemNode = new(ptr)FreeMemSingleNode(size);
        return freeMemNode;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    size_t getSize() {
        return size;
    }

    FreeMemSingleNode *getNext() {
        return const_cast<FreeMemSingleNode *>(this->next);
    }

};

class FreeMemSingleList {
private:
    FreeMemSingleNode head;
private:
public:
    void insertIntoHead(FreeMemSingleNode *node) {
        this->head.insertNext(node);
    }

    void insertIntoHead(void *ptr, size_t size) {
//        fprintf(stderr, "free node list insert head\n");
        FreeMemSingleNode *node = FreeMemSingleNode::create(ptr, size);
        this->insertIntoHead(node);
    }

    void remove(FreeMemSingleNode *preNode, FreeMemSingleNode *node) {
//        fprintf(stderr, "free node list remove\n");
        node->remove(preNode);
    }

    void replace(FreeMemSingleNode *preNode, FreeMemSingleNode *originNode, void *ptr, size_t size) {
//        fprintf(stderr, "free node list replace\n");
        FreeMemSingleNode *newNode = FreeMemSingleNode::create(ptr, size);
        this->replace(preNode, originNode, newNode);
    }

    void replace(FreeMemSingleNode *preNode, FreeMemSingleNode *originNode, FreeMemSingleNode *newNode) {
        preNode->next = newNode;
        newNode->next = originNode->next;
    }

    FreeMemSingleNode *getHead() {
        return &head;
    }

    bool isEmpty() {
        if (head.next == NULL) {
            return true;
        }
        return false;
    }
};


#endif //NUMALLOC_FREEMEMSINGLELIST_HH
