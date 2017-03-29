/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototpyes or constants in a header, please put them
 * in another header file.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef SFMM_H
#define SFMM_H
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define ALLOC_SIZE_BITS 4
#define BLOCK_SIZE_BITS 28
#define REQST_SIZE_BITS 32

#define SF_HEADER_SIZE ((ALLOC_SIZE_BITS + BLOCK_SIZE_BITS + REQST_SIZE_BITS) >> 3)
#define SF_FOOTER_SIZE SF_HEADER_SIZE

/*
            Format of a memory block
    +------------------------------------+
    |            64-bits wide            |
    +------------------------------------+

    +----------------+------------+------+    ------------
    | Requested Size | Block Size | 000a |    Header Block
    |     in bytes   |  in bytes  |      |
    |     32bits     |   28bits   | 4bits|
    +----------------+------------+------+    ------------
    |                                    |    Content of
    |         Payload and Padding        |    the payload
    |           (N Memory Rows)          |
    |                                    |
    |                                    |
    +---------------+-------------+------+    ------------
    |     Unused    | Block Size  | 000a |    Footer Block
    |               |  in bytes   |      |
    +------------------------------------+    ------------

*/

struct __attribute__((__packed__)) sf_header{
    uint64_t alloc : ALLOC_SIZE_BITS;
    uint64_t block_size : BLOCK_SIZE_BITS;
    uint64_t requested_size : REQST_SIZE_BITS;
};
typedef struct sf_header sf_header;

struct __attribute__((__packed__)) sf_free_header {
    sf_header header;
    struct sf_free_header *next;
    struct sf_free_header *prev;
};
typedef struct sf_free_header sf_free_header;

struct __attribute__((__packed__)) sf_footer {
    uint64_t alloc : ALLOC_SIZE_BITS;
    uint64_t block_size : BLOCK_SIZE_BITS;
    /* Other 32-bits are unused */
};
typedef struct sf_footer sf_footer;

/**
 * You should store the head of your free list in this variable.
 */
extern sf_free_header *freelist_head;

/* sfmm.c: Where you will define your functions for this assignment. */

/**
 * This is your implementation of malloc. It creates dynamic memory which
 * is aligned and padded properly for the underlying system. This memory
 * is uninitialized.
 * @param size The number of bytes requested to be allocated.
 * @return If successful, the pointer to a valid region of memory
 * to use is returned, else the value NULL is returned and the
 * ERRNO is set accordingly. If size is set to zero, then the
 * value NULL is returned.
 */
void* sf_malloc(size_t size);

/**
 * Marks a dynamically allocated region as no longer in use.
 * @param ptr Address of memory returned by the function sf_malloc,
 * sf_realloc, or sf_calloc.
 */
void sf_free(void *ptr);

/**
 * Resizes the memory pointed to by ptr to be size bytes.
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 * @return If successful, the pointer to a valid region
 * of memory to use is returned, else the value NULL is
 * returned and the ERRNO is set accordingly.
 *
 * A realloc call with a size of zero should return NULL
 * and set the ERRNO accordingly.
 */
void* sf_realloc(void *ptr, size_t size);

/**
 * Allocate an array of nmemb elements each of size bytes.
 * The memory returned is additionally zeroed out.
 * @param nmemb Number of elements in the array.
 * @param size The size of bytes of each element.
 * @return If successful, returns the pointer to a valid
 * region of memory to use, else the value NULL is returned
 * and the ERRNO is set accordingly. If nmemb or
 * size is set to zero, then the value NULL is returned.
 */
void* sf_calloc(size_t nmemb, size_t size);

/* sfutil.c: Helper functions already created for this assignment. */

/**
 * This routine will initialize your memory allocator. It should be called
 * in your program one time only, before using any of the other sfmm functions.
 * @param max_heap_size Unsigned value determining the maximum size of your heap.
 */
void sf_mem_init(size_t max_heap_size);

/**
 * Extends the heap by incr bytes and returns the start address of the new area.
 * You are unable to shrink the heap using this function.
 * @param increment The amount of bytes to increase the size of the heap by.
 * @return Returns the starting address of the new area.
 */
void* sf_sbrk(size_t increment);

/**
 * Function which outputs the state of the free-list to stdout.
 * See sf_snapshot section for details on output format.
 * @param verbose If true, enables snapshot to print out each memory block
 * using the sf_blockprint function. Also checks for making sure the header and
 * footer are correct, and the memory payload is correctly aligned will be performed.
 */
void sf_snapshot(bool verbose);

/**
 * Function which prints human readable block format readable format.
 * @param block Address of the block header in memory.
 */
void sf_blockprint(void *block);

/**
 * Prints human readable block format from the address of the payload.
 * IE. subtracts header size from the data pointer to obtain the address
 * of the block header. Calls sf_blockprint internally to print.
 * @param data Pointer to payload data in memory (value returned by
 * sf_malloc).
 */
void sf_varprint(void *data);

#endif
