#include "balloc.h"

/*!
 * \file
 * \brief implement the bitmap allocator interface
 */

#include <stddef.h>

// Global array of bitmap allocators
struct bitmap_alloc *bitmap_allocators = NULL;

// Number of bitmap allocators in the global array
size_t num_bitmap_allocators = 0;

void balloc_setup(void)
{
    // Optional: Place logic which should happen befor a benchmark
}
void balloc_teardown(void)
{
    // Optional: Place logic which should happen after a benchmark
}

void *alloc_block_in_bitmap(struct bitmap_alloc *alloc)
{
    (void)alloc;
    // TODO: Implement

    size_t chunk_size = alloc->chunk_size;

    for (size_t i = 0; i <= NUM_BITS_SIZE_T - 1; ++i)
    {
        if ((alloc->occupied_areas & (1 << i)) == 0)
        {
            alloc->occupied_areas |= (1 << i);

            return (char *)alloc->memory + i * chunk_size;
        }
    }

    // end
    return NULL;
}

void dealloc_block_in_bitmap(struct bitmap_alloc *alloc, void *object)
{
    (void)alloc;
    (void)object;

    size_t chunk_size = alloc->chunk_size;

    // TODO: Implement
}

void *alloc_from_os(size_t size)
{
    (void)size;
    // TODO: Implement
    return NULL;
}

void dealloc_to_os(void *memory, size_t size)
{
    (void)memory;
    (void)size;
    // TODO: Implement
}

void *alloc(size_t size)
{
    (void)size;
    // TODO: Implement
    return NULL;
}

void dealloc(void *memory)
{
    (void)memory;
    // TODO: Implement
}
