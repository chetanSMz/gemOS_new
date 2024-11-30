#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// #define NUM 4
// #define _1GB (1024*1024*1024)
#define MIN_CHUNK_SIZE (4 * 1024 * 1024)

struct FreeChunk {
    unsigned long size;
    struct FreeChunk* next;
    struct FreeChunk* prev;
};

static struct FreeChunk* freeList = NULL;

// need to change this completely
// need to point the freelist head pointer to this memory
// no need to return any value
// make the size changes too so that it will be a nearest multiple of 4
// and add the size field, next and prev pointer field to it

unsigned long allocateFromOS(unsigned long size) {
    unsigned long required_size = size;

    if(size > MIN_CHUNK_SIZE){
        unsigned long padding = MIN_CHUNK_SIZE - (size % MIN_CHUNK_SIZE);
        required_size += padding;
    }

    void* memory = mmap(NULL, required_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    if(memory == MAP_FAILED){
        return 1;
    }

    struct FreeChunk* newMemory = (struct FreeChunk*)memory;

    // insert newMemory at the head of the free list
    newMemory->size = required_size;
    newMemory->prev = NULL;
    newMemory->next = freeList;
    
    if (freeList != NULL) {
        freeList->prev = newMemory;
    }
    
    freeList = newMemory;

return 0;
}

// Function to split a free chunk into two smaller chunks
void splitChunk(struct FreeChunk* chunk, unsigned long size) {    
    struct FreeChunk* newChunk = (struct FreeChunk*)((char*)chunk + size);
    
    // insert newChunk at the head of the free list
    newChunk->size = chunk->size - size;
    newChunk->prev = NULL;
    newChunk->next = freeList;
    
    if (freeList != NULL) {
        freeList->prev = newChunk;
    }
    
    freeList = newChunk;
    
    chunk->size = size;    
}

// Function to merge adjacent free chunks
// void mergeChunks(struct FreeChunk* chunk) {
//     while (chunk->next != NULL && ((char*)chunk + chunk->size) == (char*)chunk->next) {
//         chunk->size += chunk->next->size;
//         chunk->next = chunk->next->next;
        
//         if (chunk->next != NULL) {
//             chunk->next->prev = chunk;
//         }
//     }
// }

// Function to allocate memory using the first-fit approach
// void* memalloc(unsigned long size) {
//     if (size == 0) {
//         return NULL;
//     }
    
//     // Find the first free chunk that is large enough to service the request
//     struct FreeChunk* current = freeList;
//     while (current != NULL) {
//         if (current->size >= size + sizeof(unsigned long) + ALIGNMENT_PADDING) {
//             // This chunk is large enough to serve the request
//             if (current->size >= size + sizeof(unsigned long) + ALIGNMENT_PADDING + sizeof(struct FreeChunk)) {
//                 // Split the chunk if it's significantly larger than needed
//                 splitChunk(current, size + sizeof(unsigned long) + ALIGNMENT_PADDING);
//             }
            
//             // Remove the chunk from the free list
//             if (current->prev != NULL) {
//                 current->prev->next = current->next;
//             } else {
//                 freeList = current->next;
//             }
//             if (current->next != NULL) {
//                 current->next->prev = current->prev;
//             }
            
//             return (void*)((char*)current + sizeof(unsigned long));
//         }
//         current = current->next;
//     }
    
//     // If no free chunk can satisfy the request, allocate a new chunk from the OS
//     void* memory = allocateFromOS(size);
//     if (memory == NULL) {
//         return NULL;
//     }
    
//     return memory;
// }

// Function to allocate memory using the first-fit approach
void* memalloc(unsigned long size) {
    if (size == 0) {
        return NULL;
    }
    
    unsigned long padding = 8 - (size % 8);
    unsigned long make24Bytes = 0;

    if(size + padding < 16){
        make24Bytes = 16 - (size + padding);
    }

    unsigned long required_size = sizeof(unsigned long) + size + padding + make24Bytes;

    unsigned long didSplit = 0;
    // Find the first free chunk that is large enough to service the request
    struct FreeChunk* current = freeList;
    while (current != NULL) {
        if (current->size >= required_size){
            // This chunk is large enough to serve the request
            unsigned long extraBytes = current->size - required_size;
            if (extraBytes >= 24) {
                // Split the chunk if 'b' is greater than or equal to 24
                splitChunk(current, required_size);
                didSplit = 1;
                break;
            } else {                                    
                // Include extraBytes as padding
                required_size += extraBytes;
            }
            
            // Remove the chunk from the free list
            if (current->prev != NULL) {
                current->prev->next = current->next;
            } else if(current -> prev == NULL){
                freeList = current->next;
            }

            if (current->next != NULL) {
                current->next->prev = current->prev;
            }
            
            unsigned long *addSize = (unsigned long *)current;
            *addSize = required_size;
            return (void*)((char*)current + sizeof(unsigned long));
        }
        current = current->next;
    }

    if(didSplit == 1){
        void *ptr1 = memalloc(size);
        return ptr1;
    }
    
    // If no free chunk can satisfy the request, allocate a new chunk from the OS

    // need to call the allocateFromOS(size) function and then again call memalloc(size)

    unsigned long allocated = allocateFromOS(required_size);

    if(allocated == 1){
        return NULL;
    }

    else{
        void *ptr2 = memalloc(size);
        return ptr2;
    }
}


// Function to free memory
int memfree(void* free_ptr) {
    if (free_ptr == NULL) {
        return -1;
    }
        
    // Get the size of the allocated memory from the metadata
    unsigned long* sizePtr = (unsigned long*)((char*)free_ptr - sizeof(unsigned long));
    unsigned long size = *sizePtr;
    
    // Add the freed memory chunk back to the free list
    struct FreeChunk* newFreeChunk = (struct FreeChunk*)sizePtr;
    newFreeChunk->size = size;

    /**/
    newFreeChunk->prev = NULL;
    newFreeChunk->next = freeList;
    
    if (freeList != NULL) {
        freeList->prev = newFreeChunk;
    }
    
    freeList = newFreeChunk;
    /**/
    
    // Merge adjacent free chunks if possible
    struct FreeChunk* left = NULL;
    struct FreeChunk* right = NULL;

    struct FreeChunk* curr = freeList;

    while(curr != NULL && (left != NULL || right != NULL)){
        if((void *)((char *)newFreeChunk + size) == (void *)curr){
            right = curr;
        }

        if((void *)((char *)curr + curr -> size) == (void *)newFreeChunk){
            left = curr;
        }
    }


                                            // MERGE CONDITIONS

    if(left != NULL && right != NULL){
        left -> size = (left -> size) + (newFreeChunk -> size) + (right -> size);

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
        left -> size = (left -> size) + (newFreeChunk -> size);

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
        newFreeChunk -> size = (newFreeChunk -> size) + (right -> size);

        // remove right from the free list
        if (right->prev != NULL) {
            right->prev->next = right->next;
        } else if(right -> prev == NULL){
            freeList = right->next;
        }

        if (right->next != NULL) {
            right->next->prev = right->prev;
        }

        // insert newFreeChunk at the head of the free list
        newFreeChunk->prev = NULL;
        newFreeChunk->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = newFreeChunk;
        }
        
        freeList = newFreeChunk;
    }

    else{
        // insert newFreeChunk at the head of the free list
        newFreeChunk->size = newFreeChunk -> size;
        newFreeChunk->prev = NULL;
        newFreeChunk->next = freeList;
        
        if (freeList != NULL) {
            freeList->prev = newFreeChunk;
        }
        
        freeList = newFreeChunk;
    }
    
    return 0;
}

int main() {
    char *p = 0;
	char *q = 0;
	unsigned long size = 0;
	
	p = (char *)memalloc(1);
	if(p == NULL)
	{
		printf("1.Testcase failed\n");
		return -1;
	}

	q = (char *)memalloc(9);
    printf("%lu\n", (unsigned long)p);
    printf("%lu\n", (unsigned long)q);
	if(q == NULL)
	{
		printf("2.Testcase failed\n");
		return -1;
	}

	if(q != p+24)
	{   
		printf("3.Testcase failed\n");
		return -1;
	}


	size = *((unsigned long*)q - 1);
	if(size != 24)
	{
		printf("4.Testcase failed\n");
		return -1;
	}


	printf("Testcase passed\n");
    return 0;
}
