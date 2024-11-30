#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define _4MB (4 * 1024 * 1024)

struct freeBlock {
    unsigned long size;
    struct freeBlock* next;
    struct freeBlock* prev;
};

static struct freeBlock* freeList = NULL;

unsigned long allocateFromOS(unsigned long size) {
    unsigned long required_size = size;

    if(required_size <= _4MB){
        required_size = _4MB;
    }

    else if(required_size > _4MB){
        unsigned long padding = 0;

        if(required_size % _4MB == 0){
            padding = 0;
        }

        else if(required_size % _4MB != 0){
            padding = _4MB - (required_size % _4MB);
        }
        required_size += padding;
    }

    void* memory = mmap(NULL, required_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    if(memory == MAP_FAILED){
        return 1;   // to indicate an error in mmap
    }

    struct freeBlock* newMemory = (struct freeBlock*)memory;

    // insert newMemory at the head of the free list
    newMemory->size = required_size;
    newMemory->next = freeList;
    newMemory->prev = NULL;
    
    if (freeList != NULL) {
        freeList->prev = newMemory;
    }
    
    freeList = newMemory;

return 0;
}

void* memalloc(unsigned long size) {
    if (size == 0) {
        return NULL;
    }
    
    unsigned long padding = 0;
    if(size % 8 != 0){
        padding = 8 - (size % 8);
    }

    else if(size % 8 == 0){
        padding = 0;
    }
    unsigned long make24Bytes = 0;

    if(size + padding <= 16){
        make24Bytes = 16 - (size + padding);
    }

    unsigned long required_size = sizeof(unsigned long) + size + padding + make24Bytes;

    struct freeBlock* current = freeList;
    while (current != NULL) {
        if (current->size >= required_size){
            unsigned long extraBytes = current->size - required_size;
            if (extraBytes >= 24) {
                struct freeBlock* newChunk = (struct freeBlock*)((char*)current + required_size);

                // split chunks
                newChunk->size = current->size - required_size;
                newChunk->next = freeList;
                newChunk->prev = NULL;
                
                if (freeList != NULL) {
                    freeList->prev = newChunk;
                }
                
                freeList = newChunk;
                
                current->size = required_size;

            } else {                                    
                required_size += extraBytes;
                current -> size = required_size;
            }
            
            // Remove the allocated block from the free list
            if (current->prev != NULL) {
                current->prev->next = current->next;
            } else if(current -> prev == NULL){
                freeList = current->next;
            }

            if (current->next != NULL) {
                current->next->prev = current->prev;
            }
            return (void*)((char*)current + sizeof(unsigned long));
        }
        current = current->next;
    }

    unsigned long allocated = allocateFromOS(required_size);

    if(allocated == 1){
        return NULL;
    }

    else{
        void *ptr1 = memalloc(size);
        return ptr1;
    }

return NULL;
}

// Function to free memory
int memfree(void* free_ptr) {
    if (free_ptr == NULL) {
        return -1;
    }
    
    unsigned long* sizePtr = (unsigned long*)((char*)free_ptr - sizeof(unsigned long));
    unsigned long size = *sizePtr;
    
    struct freeBlock* new_free_block = (struct freeBlock*)sizePtr;
    new_free_block->size = size;
    
    // merge adjacent free blocks
    struct freeBlock* left = NULL;
    struct freeBlock* right = NULL;

    struct freeBlock* curr = freeList;

    while(curr != NULL && (left == NULL || right == NULL)){
        if((void *)((char *)new_free_block + size) == (void *)curr){
            right = curr;
        }

        if((void *)((char *)curr + curr -> size) == (void *)new_free_block){
            left = curr;
        }
        curr = curr -> next;
    }


                                            // MERGE CONDITIONS

    if(left != NULL && right != NULL){
        left -> size = (left -> size) + (new_free_block -> size) + (right -> size);

        // remove right from the free list
        if (right->prev != NULL) {
            right->prev->next = right->next;
        } else if(right -> prev == NULL){
            freeList = right->next;
        }

        if (right->next != NULL) {
            right->next->prev = right->prev;
        }

        // remove left from the free list
        if (left->prev != NULL) {
            left->prev->next = left->next;
        } else if(left -> prev == NULL){
            freeList = left->next;
        }

        if (left->next != NULL) {
            left->next->prev = left->prev;
        }


        // insert left at the head of the free list
        left->prev = NULL;
        left->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = left;
        }
        
        freeList = left;        
    }

    else if(left != NULL){
        left -> size = (left -> size) + (new_free_block -> size);

        // remove left from the free list
        if (left->prev != NULL) {
            left->prev->next = left->next;
        } else if(left -> prev == NULL){
            freeList = left->next;
        }

        if (left->next != NULL) {
            left->next->prev = left->prev;
        }

        // insert left at the head of the free list
        left->prev = NULL;
        left->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = left;
        }
    }

    else if(right != NULL){
        new_free_block -> size = (new_free_block -> size) + (right -> size);

        // remove right from the free list
        if (right->prev != NULL) {
            right->prev->next = right->next;
        } else if(right -> prev == NULL){
            freeList = right->next;
        }

        if (right->next != NULL) {
            right->next->prev = right->prev;
        }

        // insert new_free_block at the head of the free list
        new_free_block->prev = NULL;
        new_free_block->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = new_free_block;
        }
        
        freeList = new_free_block;
    }

    else{
        // insert new_free_block at the head of the free list
        new_free_block->size = new_free_block -> size;
        new_free_block->prev = NULL;
        new_free_block->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = new_free_block;
        }
        
        freeList = new_free_block;
    }
    
    return 0;
}
