#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// Structure to represent a free memory chunk
struct FreeChunk {
    unsigned long size;
    struct FreeChunk* next;
    struct FreeChunk* prev;
};

// Global variable for the head of the free list
struct FreeChunk* freeList = NULL;



void splitChunk(struct FreeChunk* chunk, unsigned long size) {

    if(chunk->size - size < 24) return; // no need to split if size if less than 24;
        // Calculate the size of the new free chunk
    unsigned long remainingSize = chunk->size - size;
    struct FreeChunk* Chunk = (struct FreeChunk*)((char*)chunk + size);
    if(!(freeList == NULL)){
        freeList->prev = Chunk;
    }

    Chunk->size = remainingSize;
    Chunk->prev = NULL;
    Chunk->next = freeList;
    freeList = Chunk;      // adding remaining size Chunk at the first of the list;
        // Update the curr chunk
    chunk->size = size;
    return;

}

void* memalloc(unsigned long size) {
    unsigned long req = size;

    if (size == 0) {
        return NULL;
    }

    size = size + 8; // adding extra 8 bytes for size field
    unsigned long paddded_size = (size + 7) & ~7; // aligning by addding padding

    if(paddded_size <=24){
        paddded_size = 24;
    }

    // Iterate through the free list to find a chunk that fits
    struct FreeChunk* curr = freeList;

    while (curr != NULL) {

        if (curr->size >= paddded_size) {
            // Remove the chunk from the free list
            splitChunk(curr, paddded_size); // split chunk if needed;

            if (curr->prev != NULL) {
                curr->prev->next = curr->next;
            } else {
                freeList = curr->next;
            }

            if (curr->next != NULL) {
                curr->next->prev = curr->prev;
            }

            char * ptr = (char*)curr + sizeof(unsigned long);
            return (void*)(ptr);
        }

        curr = curr->next;
    }

    // If no suitable chunk is found, allocate a new one from the OS
    unsigned long aligned_size = (size + (4 * 1024 * 1024 - 1)) & ~(4 * 1024 * 1024 - 1); 

    void* ptr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) {
        // perror("mmap");
        return NULL;
    }

    struct FreeChunk* newFreeChunk = (struct FreeChunk*)ptr;
    if(!(freeList == NULL)){
        freeList->prev = newFreeChunk;
    }
    newFreeChunk->size = aligned_size;  // Subtract metadata size
    newFreeChunk->prev = NULL;
    newFreeChunk->next = freeList; // adding at the front of freelist
    freeList = newFreeChunk;
    
    memalloc(req);

}



int memfree(void* free_ptr) {

    if (free_ptr == NULL) {
        return -1;
    }

    // Calculate the address of the header
    struct FreeChunk* header = (struct FreeChunk*)((char*)free_ptr - sizeof(unsigned long));

    // Merge with left and right neighbors if possible
    struct FreeChunk* curr = freeList;
    struct FreeChunk* prev = NULL;

    while (curr != NULL) {
        if ((char*)curr + curr->size == (char*)header) {
            // Merge with right neighbor
            curr->size += sizeof(struct FreeChunk) + header->size;

            if (prev != NULL) {
                prev->next = curr->next;
                if (curr->next != NULL) {
                    curr->next->prev = prev;
                }
            } else {
                // Update freeList if merging with the first block
                freeList = curr;
                curr->prev = NULL;
            }

            header = curr; // Updated header after merging
        } else if ((char*)header + header->size == (char*)curr) {
            // Merge with left neighbor
            header->size += sizeof(struct FreeChunk) + curr->size;

            if (curr->next != NULL) {
                curr->next->prev = header;
            }

            header->next = curr->next;
        }

        prev = curr;
        curr = curr->next;
    }

    // Insert the merged block into the free list
    header->prev = NULL;
    header->next = freeList;
    
    if (freeList != NULL) {
        freeList->prev = header;
    }

    freeList = header;

    return 0;
}

// #define NUM 4
// #define _1GB (1024*1024*1024)
#define NUM 4
#define _1GB (1024*1024*1024)

//Handling large allocations
int main()
{
	char *p[NUM];
	char *q = 0;
	int ret = 0;
	int a = 0;

	for(int i = 0; i < NUM; i++)
	{
		p[i] = (char*)memalloc(_1GB);
		if((p[i] == NULL) || (p[i] == (char*)-1))
		{
			printf("1.Testcase failed\n");
			return -1;
		}

		for(int j = 0; j < _1GB; j++)
		{
			p[i][j] = 'a';
		}
	}
	
	for(int i = 0; i < NUM; i++)
	{
		ret = memfree(p[i]);
		if(ret != 0)
		{
			printf("2.Testcase failed\n");
			return -1;
		}
	}

	printf("Testcase passed\n");
	return 0;
}



