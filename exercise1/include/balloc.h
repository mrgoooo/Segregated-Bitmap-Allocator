/*!
 * \file
 * \brief declare the bitmap allocator interface
 */

#ifndef DEFINED_P1_BITMAP_ALLOC_H
#define DEFINED_P1_BITMAP_ALLOC_H

#include <stddef.h>
#include <limits.h>


//! constant for number of bits per size_t
#define NUM_BITS_SIZE_T (CHAR_BIT * sizeof(size_t))
//! calculate the size of memory managed by one bitmap_alloc
//! macro because we dont have anything better for compile time functions
#define MEMORY_SIZE_CHUNK(chunk_size) ((chunk_size) * NUM_BITS_SIZE_T)

//! minimum size of memory to allocate from the os
#define BITMAP_PAGE_SIZE 4096

//! minimum alingment for all adresses returned by the allocator
#define BALLOC_ALIGNMENT 8
#define BALLOC_ALIGNMENT_BITS 3

/*!
 * \brief struct representing a bitmap allocator for a memory area
 * 
 * Each bitmap allocator manages a memory area divided into fixed-size chunks.
 * A bitmask tracks which chunks are allocated or free.
 */
struct bitmap_alloc {
    //! size of each chunk in the memory area
    size_t chunk_size;

    //! bitmask indicating which chunks are occupied (1) or free (0)
    size_t occupied_areas;

    //! pointer to the memory area managed by this allocator
    //! the total size is chunk_size * number of bits in size_t
    void *memory;
};

//! Global array of bitmap allocators
extern struct bitmap_alloc *bitmap_allocators;

//! Number of bitmap allocators in the global array
extern size_t num_bitmap_allocators;

// Setup/Teardown functions
// Called before the first allocation
void balloc_setup(void);
// Called after the last allocation
void balloc_teardown(void);

/*!
 * \brief allocate a chunk from a specific bitmap allocator
 * 
 * Finds a free chunk in the given allocator, marks it as occupied, and returns a pointer to it.
 * \param alloc the bitmap allocator to allocate from
 * \return pointer to the allocated chunk, or NULL if no free chunks are available
 */
void *alloc_block_in_bitmap(struct bitmap_alloc *alloc);

/*!
 * \brief deallocate a chunk in a bitmap allocator
 * 
 * Finds which chunk contains the given pointer and marks it as free.
 * \param alloc the bitmap allocator containing the chunk
 * \param object pointer to the memory to be freed
 */
void dealloc_block_in_bitmap(struct bitmap_alloc *alloc, void *object);

/*!
 * \brief allocate a new memory block from the operating system
 * 
 * \param size the size of the memory block to allocate
 * \return pointer to the allocated memory, or NULL if allocation failed
 * \attention the os usually expects memory request of certain sizes...
 */
void *alloc_from_os(size_t size);

/*!
 * \brief deallocate memory back to the operating system
 * 
 * \param memory pointer to the memory to be freed
 * \param size size of the memory to be freed
 */
void dealloc_to_os(void *memory, size_t size);

/*!
 * \brief user-facing API to allocate memory
 * 
 * Allocates memory of at least the requested size, potentially using an existing
 * bitmap allocator or creating a new one if necessary.
 * \param size number of bytes to be (at least) allocated
 * \return pointer to the allocated memory, or NULL if allocation failed
 * \attention similarly to malloc(), you need to gracefully handle 0 bytes AND the pointer needs to be deallocable()
 */
void *alloc(size_t size);

/*!
 * \brief user-facing API to free previously allocated memory
 * 
 * \param memory pointer to the memory to be freed
 * \attention similarly to free(), you need to gracefully handle NULL
 */
void dealloc(void *memory);

#endif /* DEFINED_P1_BITMAP_ALLOC_H */
