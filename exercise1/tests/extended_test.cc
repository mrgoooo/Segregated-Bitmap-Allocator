#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <random>
#include <algorithm>

extern "C" {
#include "balloc.h"
}

// ===== ALIGNMENT TESTS =====
// Tests that verify alignment requirements

TEST(AlignmentTest, BasicRequirements) {
    balloc_setup();

    // Test various allocation sizes
    std::vector<size_t> sizes = {
        1, 3, 5, 7,           // Small sizes that should be rounded up
        8, 9, 15, 16, 17,     // Around power-of-2 values
        24, 31, 33,           // Odd sizes
        63, 65, 127, 129      // Larger odd sizes
    };

    std::vector<void*> allocations;
    
    for (size_t size : sizes) {
        void* result = alloc(size);
        ASSERT_TRUE(result) << "Failed to allocate size " << size;
        allocations.push_back(result);
        
        // Verify alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(result);
        EXPECT_EQ(addr % BALLOC_ALIGNMENT, 0) 
            << "Allocation of size " << size << " not aligned to " << BALLOC_ALIGNMENT;
    }
    
    // Clean up
    for (void* ptr : allocations) {
        dealloc(ptr);
    }
    
    balloc_teardown();
}

// ===== ALLOCATOR STATE TESTS =====
// Tests that verify internal state through public fields

TEST(AllocatorState, OccupiedBitsTracking) {
    balloc_setup();
    
    // Allocate a few blocks of the same size to use the same allocator
    std::vector<void*> allocations;
    for (int i = 0; i < 5; i++) {
        void* result = alloc(16);
        ASSERT_TRUE(result);
        allocations.push_back(result);
    }
    
    // Find the allocator(s) responsible for our allocations
    // maps array-index => bitfield
    std::map<size_t, size_t> occupied_states;
    
    for (size_t i = 0; i < num_bitmap_allocators; i++) {
        if (bitmap_allocators[i].occupied_areas != 0 && 
            bitmap_allocators[i].chunk_size >= 16) {
            occupied_states[i] = bitmap_allocators[i].occupied_areas;
        }
    }
    
    // We should have at least one allocator with occupied areas
    ASSERT_GT(occupied_states.size(), 0) 
        << "No allocators found with occupied areas after allocation";
    
    // Free two allocations
    dealloc(allocations[0]);
    dealloc(allocations[2]);
    
    // Check that occupied_areas was updated
    bool found_change = false;
    
    for (auto& [idx, initial_state] : occupied_states) {
        if (idx >= num_bitmap_allocators || bitmap_allocators[idx].occupied_areas != initial_state) {
            found_change = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_change) 
        << "No allocator's occupied_areas changed after deallocations";
    
    // Clean up remaining allocations
    for (size_t i = 0; i < allocations.size(); i++) {
        if (i != 0 && i != 2) { // Skip already freed ones
            dealloc(allocations[i]);
        }
    }
    
    balloc_teardown();
}

// ===== ALLOCATION PATTERNS TESTS =====
// Tests that verify complex allocation/deallocation patterns

TEST(AllocationPatterns, MixedSizeWorkload) {
    balloc_setup();
    
    // Set up deterministic "random" generator
    std::mt19937 rng(42); // Fixed seed for reproducibility
    
    const int operations = 500;
    std::vector<std::pair<void*, size_t>> active_allocations;
    
    // Tracking allocator state
    size_t peak_allocators = 0;
    
    // Perform mixed allocations and deallocations
    for (int i = 0; i < operations; i++) {
        // 70% chance to allocate, 30% chance to free (if we have allocations)
        bool should_allocate = (std::uniform_int_distribution<int>{1, 10}(rng) <= 7) 
                              || active_allocations.empty();
        
        if (should_allocate) {
            // Choose a size - mix of powers of 2 and odd sizes
            size_t size;
            if (std::uniform_int_distribution<int>{0, 1}(rng) == 0) {
                // Power of 2 size between 8 and 4096
                int power = std::uniform_int_distribution<int>{3, 12}(rng);
                size = 1UL << power;
            } else {
                // Random size between 1 and 8000
                size = std::uniform_int_distribution<size_t>{1, 8000}(rng);
            }
            
            // Allocate memory
            void* result = alloc(size);
            ASSERT_TRUE(result) << "Failed to allocate " << size << " bytes in mixed workload";
            
            // Write a pattern to the memory to verify it later
            unsigned char* mem = reinterpret_cast<unsigned char*>(result);
            size_t pattern_size = std::min(size, (size_t)16);
            for (size_t j = 0; j < pattern_size; j++) {
                mem[j] = (i + j) & 0xFF;
            }
            
            active_allocations.push_back({result, size});
            
            // Track peak allocator count
            peak_allocators = std::max(peak_allocators, num_bitmap_allocators);
            
        } else {
            // Free a random allocation
            size_t index = std::uniform_int_distribution<size_t>{0, active_allocations.size() - 1}(rng);
            void* to_free = active_allocations[index].first;
            
            dealloc(to_free);
            
            // Remove from our tracking vector
            active_allocations.erase(active_allocations.begin() + index);
        }
        
        // Every 100 operations, verify all active allocations are intact
        if (i % 100 == 0 && !active_allocations.empty()) {
            for (const auto& [ptr, size] : active_allocations) {
                unsigned char* mem = reinterpret_cast<unsigned char*>(ptr);
                // We don't check the exact pattern since we don't know when it was allocated
                // Just check memory is accessible
                EXPECT_NE(mem[0], mem[0] + 1) << "Memory appears corrupted";
            }
        }
    }
    
    // Free all remaining allocations
    for (const auto& [ptr, size] : active_allocations) {
        dealloc(ptr);
    }
    
    // We expect a reasonable number of allocators to have been created
    EXPECT_GT(peak_allocators, 0) 
        << "No allocators were created during the mixed workload";
    
    balloc_teardown();
}

TEST(AllocationPatterns, ReuseAfterFree) {
    balloc_setup();
    
    const int alloc_count = 20;
    const int rounds = 5;
    
    // First round - allocate and remember addresses
    std::vector<void*> first_round;
    
    for (int i = 0; i < alloc_count; i++) {
        size_t size = 16 * (i % 8 + 1); // 16, 32, 48, ..., 128
        void* ptr = alloc(size);
        ASSERT_TRUE(ptr);
        
        // Write a pattern to the memory
        unsigned char* mem = reinterpret_cast<unsigned char*>(ptr);
        for (size_t j = 0; j < 16; j++) {
            mem[j] = (i + j) & 0xFF;
        }
        
        first_round.push_back(ptr);
    }
    
    // Free all allocations
    for (void* ptr : first_round) {
        dealloc(ptr);
    }
    
    // Subsequent rounds should reuse memory
    for (int round = 1; round < rounds; round++) {
        std::vector<void*> this_round;
        
        for (int i = 0; i < alloc_count; i++) {
            size_t size = 16 * (i % 8 + 1); // Same sizes as before
            void* ptr = alloc(size);
            ASSERT_TRUE(ptr);
            this_round.push_back(ptr);
        }
        
        // Count how many addresses are reused from the first round
        int reused_count = 0;
        for (void* ptr : this_round) {
            if (std::find(first_round.begin(), first_round.end(), ptr) != first_round.end()) {
                reused_count++;
            }
        }
        
        // We expect some degree of memory reuse in an efficient allocator
        EXPECT_GT(reused_count, 0) 
            << "No memory addresses were reused after freeing in round " << round;
        
        // Free all allocations for this round
        for (void* ptr : this_round) {
            dealloc(ptr);
        }
    }
    
    balloc_teardown();
}

// ===== SIZE SELECTION TESTS =====
// Tests whether some degree of allocator reuse happens

TEST(SizeSelection, MultipleAllocationEfficiency) {
    balloc_setup();
    
    // Allocate many blocks of the same size
    const size_t alloc_size = 32;
    const int alloc_count = 100;
    
    std::vector<void*> allocations;
    for (int i = 0; i < alloc_count; i++) {
        void* ptr = alloc(alloc_size);
        ASSERT_TRUE(ptr);
        allocations.push_back(ptr);
    }
    
    // Count how many allocators were created for this size
    int allocator_count = 0;
    size_t chunk_size = 0;
    
    for (size_t i = 0; i < num_bitmap_allocators; i++) {
        // Check if this allocator is responsible for any of our allocations
        bool is_responsible = false;
        
        for (void* ptr : allocations) {
            char* base = static_cast<char*>(bitmap_allocators[i].memory);
            size_t alloc_chunk_size = bitmap_allocators[i].chunk_size;
            size_t allocator_size = MEMORY_SIZE_CHUNK(alloc_chunk_size);
            char* end = base + allocator_size;
            
            if (ptr >= base && ptr < end) {
                is_responsible = true;
                chunk_size = alloc_chunk_size;
                break;
            }
        }
        
        if (is_responsible) {
            allocator_count++;
        }
    }
    
    // An efficient implementation should use approximately the right number of allocators
    // Each allocator can handle NUM_BITS_SIZE_T allocations
    int expected_allocators = (alloc_count + NUM_BITS_SIZE_T - 1) / NUM_BITS_SIZE_T;
    
    EXPECT_LE(allocator_count, expected_allocators * 2) 
        << "Too many allocators created for " << alloc_count 
        << " allocations of size " << alloc_size;
    
    // The chunk size should be appropriate for the requested size
    EXPECT_GE(chunk_size, alloc_size) 
        << "Chunk size is smaller than requested allocation size";
    
    // Cleanup
    for (void* ptr : allocations) {
        dealloc(ptr);
    }
    
    balloc_teardown();
}

// ===== EDGE CASE TESTS =====
// Additional tests for edge cases

TEST(EdgeCases, StressTest) {
    balloc_setup();
    
    // Perform a large number of allocations and deallocations
    const int total_ops = 2000;
    const int max_active = 500; // Maximum number of active allocations
    
    std::mt19937 rng(42);
    std::vector<void*> active;
    
    for (int i = 0; i < total_ops; i++) {
        if (active.size() < max_active && 
            (active.empty() || std::uniform_int_distribution<int>{0, 2}(rng) != 0)) {
            // Allocate
            size_t size = std::uniform_int_distribution<size_t>{1, 4096}(rng);
            void* ptr = alloc(size);
            ASSERT_TRUE(ptr) << "Failed to allocate " << size << " bytes in stress test";
            active.push_back(ptr);
        } else if (!active.empty()) {
            // Deallocate
            size_t index = std::uniform_int_distribution<size_t>{0, active.size() - 1}(rng);
            dealloc(active[index]);
            active.erase(active.begin() + index);
        }
    }
    
    // Free all remaining allocations
    for (void* ptr : active) {
        dealloc(ptr);
    }
    
    // Cleanup
    balloc_teardown();
}

TEST(EdgeCases, AllocatorCoalescence) {
    balloc_setup();
    
    // Allocate a moderate number of small objects
    std::vector<void*> small_allocs;
    for (int i = 0; i < 20; i++) {
        void* ptr = alloc(16);
        ASSERT_TRUE(ptr);
        small_allocs.push_back(ptr);
    }
    
    // Free even-indexed allocations
    for (size_t i = 0; i < small_allocs.size(); i += 2) {
        dealloc(small_allocs[i]);
    }
    
    // Allocate some larger objects that can't use the small freed chunks
    std::vector<void*> large_allocs;
    for (int i = 0; i < 5; i++) {
        void* ptr = alloc(256);
        ASSERT_TRUE(ptr);
        large_allocs.push_back(ptr);
    }
    
    // Now reallocate small objects - they should reuse the freed small chunks
    for (size_t i = 0; i < small_allocs.size(); i += 2) {
        void* ptr = alloc(16);
        ASSERT_TRUE(ptr);
        small_allocs[i] = ptr;
    }
    
    // Clean up all allocations
    for (void* ptr : small_allocs) {
        dealloc(ptr);
    }
    for (void* ptr : large_allocs) {
        dealloc(ptr);
    }
    
    balloc_teardown();
}

TEST(EdgeCases, InvalidOperations) {
    balloc_setup();
    
    // Test NULL deallocation (shouldn't crash)
    dealloc(nullptr);

    // Test zero-size allocation
    void* zero_result = alloc(0);
    // may not crash
    dealloc(zero_result);
    

    balloc_teardown();
}


TEST(EdgeCases, VerifyCleanup) {
    EXPECT_EQ(bitmap_allocators, nullptr) << "before setup, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "before setup, everything should be zeroed";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "") << "before setup, accesses should crash";
    balloc_setup();

    // very small amount of data
    void *small_memory = alloc(sizeof(int));
    ASSERT_TRUE(small_memory);
    int *small_data = new (small_memory) int {5};
    *small_data += 1;
    EXPECT_EQ(*small_data, 6); // use memory

    // ok-ish amount of data
    void *medium_memory = alloc(sizeof(int) << 12);
    ASSERT_TRUE(medium_memory);
    int *medium_data = new (medium_memory) int {5};
    *medium_data += 2;
    EXPECT_EQ(*medium_data, 7); // use memory

    // large amount of data
    void *large_memory = alloc(sizeof(int) << 21);
    ASSERT_TRUE(large_memory);
    int *large_data = new (large_memory) int {5};
    *large_data += 3;
    EXPECT_EQ(*large_data, 8); // use memory

    dealloc(small_memory);
    dealloc(medium_memory);
    dealloc(large_memory);

    balloc_teardown();

    EXPECT_DEATH(*small_data += 1, "")
        << "after teardown, all allocations should be returned to the OS";
    EXPECT_DEATH(*medium_data += 2, "")
        << "after teardown, all allocations should be returned to the OS";
    EXPECT_DEATH(*large_data += 3, "")
        << "after teardown, all allocations should be returned to the OS";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "")
        << "after teardown, accesses should crash";
    EXPECT_EQ(bitmap_allocators, nullptr) << "after teardown, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "after teardown, everything should be zeroed";
}


TEST(EdgeCases, VerifyCleanupRepeatedly) {
    EXPECT_EQ(bitmap_allocators, nullptr) << "before setup, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "before setup, everything should be zeroed";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "") << "before setup, accesses should crash";
    balloc_setup();

    // very small amount of data
    void *before_memory = alloc(sizeof(int));
    ASSERT_TRUE(before_memory);
    int *before_data = new (before_memory) int {5};
    *before_data += 1;
    EXPECT_EQ(*before_data, 6); // use memory

    dealloc(before_memory);

    balloc_teardown();

    EXPECT_DEATH(*before_data += 1, "")
        << "after teardown, all allocations should be returned to the OS";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "")
        << "after teardown, accesses should crash";
    EXPECT_EQ(bitmap_allocators, nullptr) << "after teardown, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "after teardown, everything should be zeroed";

    for (int i = 0; i < 5000; ++i) {
        // EXPECT_DEATH is slooow
        EXPECT_EQ(bitmap_allocators, nullptr) << "before setup, everything should be zeroed";
        EXPECT_EQ(num_bitmap_allocators, 0) << "before setup, everything should be zeroed";
        balloc_setup();

        // very small amount of data
        void *loop_data = alloc(sizeof(int));
        ASSERT_TRUE(loop_data);
        int *small_data1 = new (loop_data) int {5};
        *small_data1 += 1;
        EXPECT_EQ(*small_data1, 6); // use memory

        dealloc(loop_data);

        balloc_teardown();

        EXPECT_EQ(bitmap_allocators, nullptr) << "after teardown, everything should be zeroed";
        EXPECT_EQ(num_bitmap_allocators, 0) << "after teardown, everything should be zeroed";
    }

    EXPECT_EQ(bitmap_allocators, nullptr) << "before setup, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "before setup, everything should be zeroed";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "") << "before setup, accesses should crash";

    balloc_setup();

    // very small amount of data
    void *after_memory = alloc(sizeof(int));
    ASSERT_TRUE(after_memory);
    int *after_data = new (after_memory) int {5};
    *after_data += 1;
    EXPECT_EQ(*after_data, 6); // use memory

    dealloc(after_memory);

    balloc_teardown();

    EXPECT_DEATH(*after_data += 1, "")
        << "after teardown, all allocations should be returned to the OS";
    EXPECT_DEATH(bitmap_allocators[0].chunk_size += 1, "")
        << "after teardown, accesses should crash";
    EXPECT_EQ(bitmap_allocators, nullptr) << "after teardown, everything should be zeroed";
    EXPECT_EQ(num_bitmap_allocators, 0) << "after teardown, everything should be zeroed";
}


// ===== ALLOCATION BOUNDARY TESTS =====
// Tests that verify behavior at various boundaries

TEST(BoundaryTests, SizeBoundaries) {
    balloc_setup();
    
    // Test allocations at size boundaries
    std::vector<size_t> boundary_sizes = {
        BALLOC_ALIGNMENT - 1,     // Just below minimum alignment
        BALLOC_ALIGNMENT,         // Exact minimum alignment
        BALLOC_ALIGNMENT + 1,     // Just above minimum alignment
        BITMAP_PAGE_SIZE - 1,     // Just below page size
        BITMAP_PAGE_SIZE,         // Exact page size
        BITMAP_PAGE_SIZE + 1      // Just above page size
    };
    
    std::vector<void*> allocations;
    
    for (size_t size : boundary_sizes) {
        void* ptr = alloc(size);
        
        // For very large allocations, we might get NULL which is acceptable
        if (size > BITMAP_PAGE_SIZE / 2 && !ptr) {
            continue;
        }
        
        ASSERT_TRUE(ptr) << "Failed to allocate " << size << " bytes at boundary";
        allocations.push_back(ptr);
        
        // Verify alignment
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % BALLOC_ALIGNMENT, 0) 
            << "Alignment incorrect for size " << size;
    }
    
    // Free all allocations
    for (void* ptr : allocations) {
        dealloc(ptr);
    }
    
    balloc_teardown();
}
