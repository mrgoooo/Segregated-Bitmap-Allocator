#include "balloc.h"

/*!
 * \file
 * \brief implement the bitmap allocator interface
 */

#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>

// Global array of bitmap allocators
struct bitmap_alloc *bitmap_allocators = NULL;

// Number of bitmap allocators in the global array
size_t num_bitmap_allocators = 0;

void balloc_setup(void)
{
    num_bitmap_allocators = 3;

    bitmap_allocators = alloc_from_os(
        num_bitmap_allocators * sizeof(struct bitmap_alloc));

    memset(bitmap_allocators, 0,
           num_bitmap_allocators * sizeof(struct bitmap_alloc));

    bitmap_allocators[0].chunk_size = 16;
    bitmap_allocators[0].memory = alloc_from_os(MEMORY_SIZE_CHUNK(16));
    bitmap_allocators[0].occupied_areas = 0;

    bitmap_allocators[1].chunk_size = 64;
    bitmap_allocators[1].memory = alloc_from_os(MEMORY_SIZE_CHUNK(64));
    bitmap_allocators[1].occupied_areas = 0;

    bitmap_allocators[2].chunk_size = 10000;
    bitmap_allocators[2].memory = alloc_from_os(MEMORY_SIZE_CHUNK(10000));
    bitmap_allocators[2].occupied_areas = 0;
}
void balloc_teardown(void)
{
    if (!bitmap_allocators)
        return;

    for (size_t i = 0; i < num_bitmap_allocators; i++)
    {
        if (bitmap_allocators[i].memory)
        {
            dealloc_to_os(bitmap_allocators[i].memory,
                          MEMORY_SIZE_CHUNK(bitmap_allocators[i].chunk_size));
            bitmap_allocators[i].memory = NULL;
        }
    }

    dealloc_to_os(bitmap_allocators,
                  num_bitmap_allocators * sizeof(struct bitmap_alloc));
    bitmap_allocators = NULL;
    num_bitmap_allocators = 0;
}

void *alloc_block_in_bitmap(struct bitmap_alloc *alloc)
{
    // TODO: Implement

    size_t chunk_size = alloc->chunk_size;

    for (size_t i = 0; i <= NUM_BITS_SIZE_T - 1; ++i)
    {
        if ((alloc->occupied_areas & ((size_t)1 << i)) == 0)
        {
            alloc->occupied_areas |= ((size_t)1 << i);

            return (char *)alloc->memory + i * chunk_size;
        }
    }

    /*
    fprintf(stderr,
            "[ALLOC WARNING] alloc_block_in_bitmap: no free chunk available (chunk_size=%zu, bitmap=%zu)\n",
            chunk_size,
            alloc->occupied_areas);
    */

    // end
    return NULL;
}

void dealloc_block_in_bitmap(struct bitmap_alloc *alloc, void *object)
{
    size_t chunk_size = alloc->chunk_size;

    // dif in bytes
    ptrdiff_t diff = (char *)object - (char *)alloc->memory;

    // max diff in bytes
    ptrdiff_t max = MEMORY_SIZE_CHUNK(chunk_size) - chunk_size;

    if (diff < 0)
        return;
    if (diff > max)
        return;
    if (diff % chunk_size != 0)
        return;

    alloc->occupied_areas = alloc->occupied_areas & ~((size_t)1 << diff / chunk_size);
}

void *alloc_from_os(size_t size)
{
    void *ptr = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (ptr == MAP_FAILED)
        return NULL;

    return ptr;
}

void dealloc_to_os(void *memory, size_t size)
{
    if (!memory || size == 0)
        return;

    munmap(memory, size);
}

void *alloc(size_t size)
{
    for (size_t i = 0; i < num_bitmap_allocators; i++)
    {
        if (size <= bitmap_allocators[i].chunk_size)
        {
            void *result = alloc_block_in_bitmap(&bitmap_allocators[i]);
            if (result)
            {
                return result;
            }
            else
            {
                // No free block in this allocator, try the next one
                continue;
            }
        }
    }
    return NULL;
}

void dealloc(void *memory)
{
    if (!memory)
        return;

    for (size_t i = 0; i < num_bitmap_allocators; i++)
    {
        struct bitmap_alloc *alloc = &bitmap_allocators[i];

        if (!alloc->memory)
            continue;

        char *start = (char *)alloc->memory;
        char *end = start + MEMORY_SIZE_CHUNK(alloc->chunk_size);

        if ((char *)memory >= start && (char *)memory < end)
        {
            dealloc_block_in_bitmap(alloc, memory);
            return;
        }
    }
}
