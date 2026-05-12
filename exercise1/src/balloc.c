#include "balloc.h"

/*!
 * \file
 * \brief implement the bitmap allocator interface
 */

#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>

// Global array of bitmap allocators
struct bitmap_alloc *bitmap_allocators = NULL;

// Number of bitmap allocators in the global array
size_t num_bitmap_allocators = 0;

static const size_t slabs[] = {
    8, 16, 32, 64, 128, 256,
    512, 1024, 2048, 4096,
    8192, 16384};

size_t closest_slab(size_t size)
{
    for (size_t i = 0; i < sizeof(slabs) / sizeof(slabs[0]); i++)
    {
        if (size <= slabs[i])
            return slabs[i];
    }
    return SIZE_MAX;
}

void balloc_setup(void)
{

    size_t total = 0;

    num_bitmap_allocators = sizeof(slabs) / sizeof(slabs[0]);

    bitmap_allocators = alloc_from_os(
        num_bitmap_allocators * sizeof(struct bitmap_alloc));

    memset(bitmap_allocators, 0,
           num_bitmap_allocators * sizeof(struct bitmap_alloc));

    for (size_t i = 0; i < num_bitmap_allocators; ++i)
    {
        bitmap_allocators[i].chunk_size = slabs[i];
        bitmap_allocators[i].memory = alloc_from_os(MEMORY_SIZE_CHUNK(slabs[i]));
        bitmap_allocators[i].occupied_areas = 0;
        total += MEMORY_SIZE_CHUNK(slabs[i]);
    }
    // printf("TOTAL: %zu bytes\n", total);
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

    size_t chunk_size = alloc->chunk_size;

    size_t free_mask = ~alloc->occupied_areas;

    if (free_mask == 0)
        return NULL;

    size_t i = __builtin_ctzll(free_mask);

    alloc->occupied_areas |= ((size_t)1 << i);

    return (char *)alloc->memory + i * chunk_size;

    /*fprintf(stderr,
            "[ALLOC WARNING] alloc_block_in_bitmap: no free chunk available (chunk_size=%zu, bitmap=%zu)\n",
            chunk_size,
            alloc->occupied_areas);

    return NULL;
    */
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
    if (size == 0)
        return NULL;

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
    size_t new_count = num_bitmap_allocators + 1;

    struct bitmap_alloc *new_arr =
        alloc_from_os(new_count * sizeof(struct bitmap_alloc));

    if (!new_arr)
    {
        printf("Failed to allocate memory for new bitmap allocator array\n");
        return NULL;
    }

    memcpy(new_arr,
           bitmap_allocators,
           num_bitmap_allocators * sizeof(struct bitmap_alloc));

    dealloc_to_os(bitmap_allocators,
                  num_bitmap_allocators * sizeof(struct bitmap_alloc));

    bitmap_allocators = new_arr;

    struct bitmap_alloc *slab = &bitmap_allocators[num_bitmap_allocators];

    // slab->chunk_size = size;
    size_t closest_slab_value = closest_slab(size);
    closest_slab_value = (closest_slab_value != SIZE_MAX) ? closest_slab_value : size;

    slab->chunk_size = closest_slab_value;
    slab->occupied_areas = 0;
    slab->memory = alloc_from_os(MEMORY_SIZE_CHUNK(closest_slab_value));

    num_bitmap_allocators++;

    return alloc_block_in_bitmap(slab);
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
