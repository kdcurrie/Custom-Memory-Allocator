/**
 * @file
 * @author: Keaton Currie
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "allocator.h"
#include "logger.h"

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL; /*!< End (tail) of our linked list */


static unsigned long g_allocations = 0; /*!< Allocation counter */
static unsigned long g_regions = 0; /*!< Allocation counter */

/*< Mutex for protecting the linked list */
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Given a free block, this function will split it into two pieces and update
 * the linked list.
 *
 * @param block the block to split
 * @param size new size of the first block after the split is complete,
 * including header sizes. The size of the second block will be the original
 * block's size minus this parameter.
 *
 * @return address of the resulting second block (the original address will be
 * unchanged) or NULL if the block cannot be split.
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{
    if(block->size < 100 || !block->free || block->size - size < 100) {
        LOGP("block is too small or not free!\n");
        return NULL;
    }

    LOGP("Making it here\n");
    struct mem_block *free_block = (void*)block + size;
    LOGP("Making it here2\n");

    free_block->size = block->size - size;
    LOG("OG block address: %p\n", block);
    LOG("free block address: %p\n", free_block);
    free_block->free = true;
    LOG("size: %zu\n", size);
    free_block->region_id = block->region_id;
    snprintf(free_block->name, 32, "Allocation %lu", g_allocations++);
 

    if(block == g_tail) {
        free_block->next = NULL;
        free_block->prev = block;
        block->next = free_block;
        g_tail = free_block;
    } else {
        free_block->prev = block;
        free_block->next = block->next;
        block->next->prev = free_block;
        block->next = free_block;
    }

    block->size = size;
    block->free = false;
    return free_block + 1;

}

/** merge_block prototype */
struct mem_block *merge_block(struct mem_block *block);

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
void *first_fit(size_t size)
{
    if(g_head == NULL) {
        return NULL;
    }
    struct mem_block *curr = g_head;
    LOGP("first fit made here?\n");
    while(curr != NULL) {
        if(curr->free && curr->size >= size) {
            LOG("curr size: %zu\n", curr->size);
            LOG("size needed: %zu\n", size);
            return (void*) curr;
        }
        curr = curr->next;
    }

    LOGP("Returning NULL from first_fit\n");
    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * worst fit free space management algorithm. If there are ties (i.e., you find
 * multiple worst fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *worst_fit(size_t size)
{
    struct mem_block *block = g_head;
    struct mem_block *curr_worst = NULL;

    while(block != NULL) {
        if(curr_worst == NULL && block->free && block->size >= size) {
            curr_worst = block;
        } else if(curr_worst != NULL) {
            if(block->free && block->size >= curr_worst->size) {
                curr_worst=block;
            }
        }
        block = block->next;
    }
    if(curr_worst) {
        return (void*)curr_worst;
    }

    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * best fit free space management algorithm. If there are ties (i.e., you find
 * multiple best fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *best_fit(size_t size)
{
    struct mem_block *block = g_head;
    struct mem_block *curr_best = NULL;

    while(block != NULL){
        if(curr_best == NULL && block->free && block->size >= size) {
            curr_best = block;
        } else if(curr_best != NULL){
            if(block->free && block->size < curr_best->size && block->size >= size){
               curr_best = block;
            }
        }
        block = block->next;
    }
    if(curr_best != NULL){
        return (void*)curr_best;
    }

    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * worst fit free space management algorithm. If there are ties (i.e., you find
 * multiple worst fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *reuse(size_t size)
{
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }
    struct mem_block *block = NULL;
    if (strcmp(algo, "first_fit") == 0) {
        LOG("first fit, reuse aligned size: %zu\n", size);
        block = first_fit(size);
        LOGP("reuse: made outside first first");
    } else if (strcmp(algo, "best_fit") == 0) {
        block = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        block = worst_fit(size);
    }

    if (block == NULL) {
        return NULL;
    }

    return block;
}

/**
 * Fills any new memory allocation with 0xAA when the scribbling environment
 * variable is set to 1.
 *
 * @param size the size of the block of memory
 * @param name the desired name for the allocation
 *
 * @return returns a void pointer to the space in memory
 */
void *malloc_name(size_t size, char* name) {
    void *ptr = malloc(size);
    struct mem_block *block = (struct mem_block *) ptr - 1;
    strcpy(block->name, name);
    return ptr;

}

/**
 * Fills any new memory allocation with 0xAA when the scribbling environment
 * variable is set to 1.
 *
 * @param block the block of memory we are scribbling
 * @param aligned the size of the block of memory
 * @param boolean indicator
 *
 */
void scribbler(struct mem_block *block, size_t aligned_size, bool scribble) {
    if(!scribble) {
        return;
    }
    char *tenten = (char*)block+100;
    
    int i = 0;
    while(i < aligned_size-100) {
        *(tenten+i) = (char)0b10101010;
        i++; 
    }
    LOG("i = %d\n", i);
    LOGP("zeroed out memory\n");
}

/**
 * Thread safe malloc
 *
 * @param size the number of bytes to allocate
 */
void *malloc(size_t size) {
    pthread_mutex_lock(&alloc_mutex);
    void *ptr = unsafe_malloc(size);
    pthread_mutex_unlock(&alloc_mutex);
    return ptr;
}
/**
 * malloc allocates size bytes and returns a pointer to the allocated memory.
 *
 * @param size the number of bytes to allocate
 *
 * @return a void pointer to allocated memory
 */
void *unsafe_malloc(size_t size)
{
    bool scribble = false/*!< Start (head) of our linked list */
;
    char *scrib = getenv("ALLOCATOR_SCRIBBLE");
    if (scrib == NULL) {
        scrib = "0";
    }
    if (strcmp(scrib, "1") == 0) {
        scribble = true;
        LOGP("scribbling enabled\n");
    } else {
        LOGP("scribbling disabled\n");
    }

    size_t total_size = size + sizeof(struct mem_block);
    size_t aligned_size = (total_size / 8) * 8;
    if((aligned_size < total_size)) {
        aligned_size = aligned_size + 8;
    }
    LOG("Allocating request; size = %zu, total size = %zu, aligned = %zu\n",
            size, total_size, aligned_size);
 
    struct mem_block *reused_block = reuse(aligned_size);
    if (reused_block != NULL) {
        LOG("Reusing block at %p\n", reused_block);
        split_block(reused_block, aligned_size);
        LOGP("are we breaking here?\n");
        reused_block->free = false;
        scribbler(reused_block, aligned_size, scribble);
        return reused_block + 1;
    }

    int page_size = getpagesize();
    size_t num_pages = aligned_size / page_size;
    if (aligned_size % page_size != 0) {
        num_pages++;
    }


    size_t region_size = num_pages * page_size;
    LOG("New region size: %zu\n", region_size);
    struct mem_block *block = mmap(
            NULL, 
            region_size,
            PROT_READ | PROT_WRITE, 
            MAP_PRIVATE | MAP_ANONYMOUS, 
            -1, 
            0);

    if(block == MAP_FAILED) {
        perror("mmap");
        return NULL;

    }

    snprintf(block->name, 32, "Allocation %lu", g_allocations++);
    block->size = region_size;
    block->free = true;
    block->region_id = g_regions++;
    LOG("region_id = %lu\n", block->region_id);

    if (g_head == NULL && g_tail == NULL) {
        LOGP("Initializing first block\n");
        block->next = NULL;
        block->prev = NULL;
        g_head = block;
        g_tail = block;
    } else {
        g_tail->next = block;
        block->prev = g_tail;
        block->next = NULL;
        g_tail = block;
    }
    if(split_block(block, aligned_size) == NULL) {
        block->free = false;
    }

    LOG("New allocation: %p; data = %p\n", block, block + 1);
    scribbler(block, aligned_size, scribble);
    //LOGP("malloc: Segging here?\n");
    return block + 1;
}

/**
 * Given a free block, this function attempts to merge it with neighboring
 * blocks --- both the previous and next neighbors --- and update the linked
 * list accordingly.
 *
 * @param block the block to merge
 *
 * @return address of the merged block or NULL if the block cannot be merged.
 */
struct mem_block *merge_block(struct mem_block *block)
{

  if(block->prev != NULL && block->prev->free 
            && (block->prev->region_id == block->region_id)) {
        LOGP("merge: making here 0\n");
        block = block->prev;
        block->size = block->size + block->next->size;
        if(block->next->next != NULL) {
            block->next->next->prev = block;
            block->next = block->next->next;

            LOGP("merge: making here 1\n");
        } else if(block->next->next == NULL) {
            block->next = NULL;
        }
    }

    if(block->next != NULL && block->next->free 
            && block->next->region_id == block->region_id) {
        LOGP("merge: making here 2\n");
        block->size = block->size + block->next->size;
        if(block->next->next != NULL) {
            block->next->next->prev = block;
            block->next = block->next->next;
            LOGP("merge: making here 3\n");

        } else {
            block->next = NULL;
        }
    } 

    LOGP("Merging block with previous\n");
    LOG("Merging memory region, size of block = %zu\n", block->size);
    return block;
}

/**
 * Given a block of memory this function will free the block and
 * update the linked list. If free is given a block of memory that represents
 * a whole region then it will unmap that region from memory.
 *
 * @param ptr the block of memory to free
 *
 */
void free(void *ptr)
{
    pthread_mutex_lock(&alloc_mutex); 

    LOG("Free request; address = %p\n", ptr);
    LOG("Free request; address - 100 = %p\n", ptr - 100);
    if (ptr == NULL) {
        LOGP("bye\n");
        pthread_mutex_unlock(&alloc_mutex);
        return;
    }

    LOGP("free: Why are we making it here?\n");
    struct mem_block *block = (struct mem_block *) ptr - 1;
    LOG("SIZE BEFORE FREE: %zu\n", block->size);
    block->free = true;
    LOGP("free: faulting here?\n");
    block = merge_block(block);
    LOG("SIZE AFTER FREE: %zu\n", block->size);

    if(block->next == NULL) {
        g_tail = block;
    }
    bool umap = false;
    if (block->next == NULL && block->prev == NULL) {
        g_head = NULL;
        g_tail = NULL;
        umap = true;

    } if (block->prev != NULL) {
        if (block->next != NULL) {
            if(block->next->region_id != block->region_id 
                    && block->prev->region_id != block->region_id) {
                umap = true;
                block->prev->next = block->next;
                block->next->prev = block->prev;
            }
        } else if(block->next == NULL) {
            if(block->prev->region_id != block->region_id) {
                umap = true;
                block->prev->next = block->next;
                g_tail = block->prev;
            }
        }

    } else if(block->next != NULL) {
        if(block->next->region_id != block->region_id) {
            umap = true;
            block->next->prev = block->prev;
            g_head = block->next;
        }
    }

    if(umap == true) {
        LOG("block->size = %zu\n", block->size);
        void *block_umap = (void*)block;
        LOG("Unmapping memory region: size = %zu\n", block->size);
        LOG("Unmapping memory region: address = %p\n", block);

        int ret = munmap(block_umap, block->size);
        if (ret == -1) {
            perror("munmap");
        }
    }
    pthread_mutex_unlock(&alloc_mutex);
    return;

}

/**
 * Given a number of members and size of each member, calloc will allocate a
 * space in memory of that size, and zeros out the memory.
 *
 * @param nmemb the number of members
 * @param size the size of the block
 *
 */
void *calloc(size_t nmemb, size_t size)
{
    pthread_mutex_lock(&alloc_mutex);
    void *ptr = unsafe_malloc(nmemb * size);
    LOG("Clearing memory at %p\n", ptr);
    memset(ptr, 0, nmemb * size);
    pthread_mutex_unlock(&alloc_mutex);
    return ptr;
}

/**
 * Given a area in memory realloc will change the size of memory to the
 * requested new size
 *
 * @param ptr the area of memory we are reallocating
 * @param size the size of the block
 *
 */
void *realloc(void *ptr, size_t size)
{
    pthread_mutex_lock(&alloc_mutex); 
    if (ptr == NULL) {
        pthread_mutex_unlock(&alloc_mutex);
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        pthread_mutex_unlock(&alloc_mutex);
        return NULL;
    }

    pthread_mutex_unlock(&alloc_mutex);
    return NULL;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{

    //fputs("-- Current Memory State --\n", stderr);
    struct mem_block *current_block = g_head;
    unsigned long curr_region = current_block->region_id;
    bool next_region_flag = true;
    while (current_block != NULL) {
        if(current_block->region_id != curr_region) {
            curr_region = current_block->region_id;
            next_region_flag = true;
        }
        if(next_region_flag) {
            next_region_flag = false;
            fprintf(stdout, "[REGION %lu] %p\n", current_block->region_id, current_block);
        }
        fprintf(stdout, "  [BLOCK] %p-%p '%s' %lu [%s]\n", current_block,
                (char*)current_block+current_block->size, current_block->name,
                current_block->size, current_block->free ? "FREE" : "USED");
         //For debugging
         /*fprintf(stderr, "    block->prev = %p\n    block->next = %p\n",
                current_block->prev != NULL ? current_block->prev  : 0,
                current_block->next != NULL ? current_block->next  : 0);
        */
        current_block = current_block->next; 
    }
}

