#include <benchmark/benchmark.h>
#include <cstddef>
#include <array>
#include <vector>

extern "C" {
#include "balloc.h"
}

static void BM_BitmapSmallAlloc(benchmark::State &state) {
    // Create a static bitmap allocator
    std::array<std::byte, 64 * sizeof(size_t)> backing_storage;
    struct bitmap_alloc alloc;
    alloc.chunk_size = 8; 
    alloc.occupied_areas = 0;
    alloc.memory = backing_storage.data();
    
    for (auto _ : state) {
        void* result = alloc_block_in_bitmap(&alloc);
        benchmark::DoNotOptimize(result);
        dealloc_block_in_bitmap(&alloc, result);
    }
}

// Equivalent to many_bytes_fixed
static void BM_BitmapManyBytes(benchmark::State &state) {
    // Use a smaller size that fits on the stack, or use heap allocation
    constexpr size_t ALLOC_SIZE = 1 << 16; // 64KB instead of 31MB
    std::vector<std::byte> backing_storage(ALLOC_SIZE);
    
    struct bitmap_alloc alloc;
    alloc.chunk_size = 1024; // Large enough for our test blocks
    alloc.occupied_areas = 0;
    alloc.memory = backing_storage.data();
    
    for (auto _ : state) {
        // Only iterate over sizes that fit in our smaller allocation
        for (long i = 1; i < 10; ++i) {
            size_t size = 1L << i | 1L << (i - 1);
            if (size > alloc.chunk_size) continue; // Skip if too large
            
            void* result = alloc_block_in_bitmap(&alloc);
            if (result) {
                *reinterpret_cast<volatile char *>(result) = 5;
                dealloc_block_in_bitmap(&alloc, result);
            }
        }
    }
}

// Equivalent to linked_list_fixed
static void BM_BitmapLinkedList(benchmark::State &state) {
    // Use heap allocation instead of stack
    std::vector<std::byte> backing_storage(1 << 16); // 64KB
    
    struct bitmap_alloc alloc;
    alloc.chunk_size = sizeof(void*); // Size of a pointer for linked list nodes
    alloc.occupied_areas = 0;
    alloc.memory = backing_storage.data();
    
    struct singly_linked_list {
        singly_linked_list *next;
    };
    
    for (auto _ : state) {
        singly_linked_list first {nullptr};
        // Limit allocations to avoid running out of space
        size_t count = 0;
        const size_t max_allocs = backing_storage.size() / alloc.chunk_size;
        
        singly_linked_list *curr = &first;
        while (count < max_allocs) {
            curr->next = reinterpret_cast<singly_linked_list *>(
                alloc_block_in_bitmap(&alloc));
            if (!curr->next) break;
            curr = curr->next;
            curr->next = nullptr;
            count++;
        }
        
        for (singly_linked_list *curr = first.next; curr;) {
            auto copy = curr;
            curr = curr->next;
            dealloc_block_in_bitmap(&alloc, copy);
        }
    }
}

BENCHMARK(BM_BitmapSmallAlloc);
BENCHMARK(BM_BitmapManyBytes);
BENCHMARK(BM_BitmapLinkedList);

BENCHMARK_MAIN();
