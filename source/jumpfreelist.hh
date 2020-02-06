//
// Created by XIN ZHAO on 2/5/20.
//

#ifndef NUMALLOC_JUMPFREELIST_H
#define NUMALLOC_JUMPFREELIST_H


#include <cassert>
#include "freelist.hh"

class JumpFreeList : protected FreeList {
private:
    const static int JUMP_LIST_SIZZE = 1000;

    long firstBatchNum = 0;   // the number of elements in this batch
    long jumpIntervals = 500;  // intervals of jump list.
    void *jumpList[JUMP_LIST_SIZZE]{};
    /**
     * jumpList is a circle.
     *
     * insert in head : head --
     * pop in head: head ++
     * insert in tail : tail ++
     * pop in tail : tail --
     *
     * if jumpHead==jumpTail: emply list.
     * if tail == head-1: full list.
     */
    long jumpHead = 0;
    long jumpTail = 0;
private:
    bool isJumpListEmpty() {
        return jumpHead == jumpTail;
    }

    bool isJumpListFULL() {
        return jumpTail == (jumpHead - 1) % JUMP_LIST_SIZZE;
    }

    void insertIntoJumpListTail(void *node) {
        assert(!this->isJumpListFULL());
        jumpList[jumpTail] = node;
        jumpTail = (jumpTail + 1) % JUMP_LIST_SIZZE;
    }

    void insertIntoJumpListHead(void *node) {
        assert(!this->isJumpListFULL());
        jumpHead--;
        if (jumpHead < 0) {
            jumpHead += JUMP_LIST_SIZZE;
        }
        jumpHead = jumpHead % JUMP_LIST_SIZZE;
        jumpList[jumpHead] = node;
    }

    void *popJumpListHead() {
        assert(!this->isJumpListEmpty());
        void *node = jumpList[jumpHead];
        jumpList[jumpHead] = NULL;
        jumpHead = (jumpHead + 1) % JUMP_LIST_SIZZE;
        return node;
    }

    void buildFreeMemBatchList(JumpFreeList *list, long length, void *head, void *tail,
                               void **jumpList,
                               int jumpListLength, long jumpIntervals, long firstBatchNum) {
        assert(jumpListLength <= JUMP_LIST_SIZZE);
        list->_list = head;
        list->_length = length;
        list->_tail = tail;
        list->jumpIntervals = jumpIntervals;
        for (long i = 0; i < jumpListLength; i++) {
            list->jumpList[i] = jumpList[i];
        }
        list->jumpHead = 0;
        list->jumpTail = jumpListLength;
        list->firstBatchNum = firstBatchNum;
    }

public:

    using FreeList::length;
    using FreeList::empty;
    using FreeList::hasItems;


    void Init(long intervals = 500) {
        FreeList::Init();
        this->jumpIntervals = intervals;
        this->firstBatchNum = 0;
        this->jumpHead = this->jumpTail = 0;
    }

    void reset() {
        this->Init(this->jumpIntervals);
    }

    void Push(void *ptr) {
        FreeList::Push(ptr);
        this->firstBatchNum++;
        if (this->empty()) {
            this->insertIntoJumpListHead(ptr);
            return;
        }
        if (this->firstBatchNum <= this->jumpIntervals) {
            return;
        }
        this->insertIntoJumpListHead(ptr);
        this->firstBatchNum = 1;
    }

    void *Pop() {
        void *node = FreeList::Pop();
        if (node == NULL) {
            return node;
        }
        this->firstBatchNum--;
        assert(this->firstBatchNum >= 0);
        if (this->firstBatchNum == 0) {
            this->firstBatchNum = this->jumpIntervals;
            this->popJumpListHead();
        }
        if (this->isJumpListEmpty()) {
            this->firstBatchNum = 0;
        }
        return node;
    }

    void PushRange(JumpFreeList *list) {
        assert(list->jumpIntervals == this->jumpIntervals);
        assert(list->jumpHead == 0);
        long jumpListLength = list->jumpTail - list->jumpHead;

        long batchNum = list->firstBatchNum;
        for (long i = 0; i < batchNum; i++) {
            this->Push(list->Pop());
        }

        FreeList::_PushRangeFromBack(list->_length, list->_list, list->_tail);

        for (int i = 1; i < jumpListLength; i++) {
            this->insertIntoJumpListTail(list->jumpList[i]);
        }

        list->reset();
    }

    void PopRange(int num, JumpFreeList *list) {
        if (this->length() < num) {
            return;
        }
        void *head = this->_list;
        void *tail = this->popJumpListHead();
        size_t total_num = this->firstBatchNum;
        int jumpListLength = 0;
        thread_local void *buf_array[JUMP_LIST_SIZZE];
        buf_array[jumpListLength] = tail;
        while (total_num < num && !this->isJumpListEmpty()) {
            jumpListLength++;
            tail = this->popJumpListHead();
            total_num += this->jumpIntervals;
            buf_array[jumpListLength] = tail;
        }
        buildFreeMemBatchList(list, total_num, head, tail, buf_array,
                              jumpListLength + 1,
                              this->jumpIntervals, this->firstBatchNum);

        if (this->isJumpListEmpty()) {
            this->reset();
            return;
        }

        this->_list = *(void **) tail;
        this->_length = this->_length - total_num;
        this->firstBatchNum = this->jumpIntervals;
    }

    long getJumpIntervals() const {
        return jumpIntervals;
    }

};


#endif //NUMALLOC_JUMPFREELIST_H
