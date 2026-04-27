#include <benchmark/benchmark.h>
#include <cstddef>
#include <vector>

extern "C" {
#include "balloc.h"
}

// Equivalent to one_page_paged
static void BM_OnePageOS(benchmark::State &state) {
    for (auto _ : state) {
        auto inner = alloc_from_os(BITMAP_PAGE_SIZE);
        dealloc_to_os(inner, BITMAP_PAGE_SIZE);
    }
}
BENCHMARK(BM_OnePageOS);

// Equivalent to many_bytes_paged
static void BM_ManyBytesOS(benchmark::State &state) {
    constexpr long NUM_BYTES = 0b1111 << 25;
    for (auto _ : state) {
        // 1 << 13 = 8192 = 2*BITMAP_PAGE_SIZE
        for (long i = 13; (1L << i) < NUM_BYTES; ++i) {
            size_t size = 1L << i | 1L << (i - 1);
            auto inner = alloc_from_os(size);
            *reinterpret_cast<volatile char *>(inner) = 5;
            dealloc_to_os(inner, size);
        }
    }
}
BENCHMARK(BM_ManyBytesOS);

struct singly_linked_list {
    singly_linked_list *next;
};

// Equivalent to linked_list_pages
static void BM_LinkedListOS(benchmark::State &state) {
    constexpr auto limit = 1 * 1024 * 1024;
    for (auto _ : state) {
        long sum = 0;
        singly_linked_list first {nullptr};
        for (singly_linked_list *curr {&first}; sum < limit; sum += BITMAP_PAGE_SIZE) {
            curr->next = reinterpret_cast<singly_linked_list *>(alloc_from_os(BITMAP_PAGE_SIZE));
            curr = curr->next;
            curr->next = nullptr;
        }
        for (singly_linked_list *curr {first.next}; curr;) {
            auto copy = curr;
            curr = curr->next;
            dealloc_to_os(copy, BITMAP_PAGE_SIZE);
        }
    }
}
BENCHMARK(BM_LinkedListOS);

BENCHMARK_MAIN();
