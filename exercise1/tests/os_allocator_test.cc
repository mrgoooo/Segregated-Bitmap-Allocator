#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "balloc.h"
}

TEST(OsAllocator, BasicAllocation) {
    size_t alloc_size = 4096;
    void* result = alloc_from_os(alloc_size);
    ASSERT_TRUE(result) << "no need to continue the test if there is no memory";
    
    // Should be able to write to the memory
    int *ptr = reinterpret_cast<int *>(result);
    *ptr = 5;
    EXPECT_EQ(*ptr, 5);
    
    // Clean up
    dealloc_to_os(result, alloc_size);
}

TEST(OsAllocator, TwoAllocations) {
    size_t alloc_size = 4096;
    void* first_result = alloc_from_os(alloc_size);
    ASSERT_TRUE(first_result);
    
    void* second_result = alloc_from_os(alloc_size);
    ASSERT_TRUE(second_result);
    
    // non-overlapping memory regions
    EXPECT_NE(first_result, second_result);
    char *fst_ptr = reinterpret_cast<char *>(first_result);
    char *sec_ptr = reinterpret_cast<char *>(second_result);
    
    if (fst_ptr < sec_ptr) {
        EXPECT_LE(fst_ptr + alloc_size, sec_ptr); // might be adjacent
    } else {
        EXPECT_LE(sec_ptr + alloc_size, fst_ptr); // might be adjacent
    }
    
    // Assert that memory regions do not communicate
    *fst_ptr = 8;
    EXPECT_EQ(*fst_ptr, 8);
    *sec_ptr = 9;
    EXPECT_EQ(*sec_ptr, 9);
    EXPECT_EQ(*fst_ptr, 8); // old value
    *fst_ptr = 10;
    EXPECT_EQ(*sec_ptr, 9);
    
    // Clean up
    dealloc_to_os(first_result, alloc_size);
    dealloc_to_os(second_result, alloc_size);
}

TEST(OsAllocator, VariousSizes) {
    // Test different allocation sizes
    std::vector<size_t> sizes = {
        3,                  // Tiny allocation
        4095,               // Just under a page
        4096,               // Exactly a page
        4097,               // Just over a page
        8192,               // Two pages
        1024 * 1024,        // 1MB
        1024 * 1024 * 10    // 10MB
    };
    
    std::vector<void*> allocations;
    
    for (size_t size : sizes) {
        void* result = alloc_from_os(size);
        ASSERT_TRUE(result) << "Failed to allocate " << size << " bytes";
        allocations.push_back(result);
        
        // Write to the memory to ensure it's usable
        char* memory = reinterpret_cast<char*>(result);
        memory[0] = 1;
        memory[size/2] = 2;
        memory[size-1] = 3;
        
        EXPECT_EQ(memory[0], 1);
        EXPECT_EQ(memory[size/2], 2);
        EXPECT_EQ(memory[size-1], 3);
    }
    
    // Clean up all allocations
    for (size_t i = 0; i < sizes.size(); i++) {
        dealloc_to_os(allocations[i], sizes[i]);
    }
}

TEST(OsAllocator, ManyAllocations) {
    std::mt19937_64 engine{std::random_device{}()};
    
    const int num_allocations = 100;
    std::vector<std::pair<void*, size_t>> allocations;
    
    for (int i = 0; i < num_allocations; i++) {
        // Allocate a random size between 1 and 100KB
        size_t size = std::uniform_int_distribution<size_t>{1, 100 * 1024}(engine);
        void* result = alloc_from_os(size);
        ASSERT_TRUE(result);
        allocations.push_back({result, size});
        
        // Write a pattern to the memory
        unsigned char* memory = reinterpret_cast<unsigned char*>(result);
        for (size_t j = 0; j < size; j++) {
            memory[j] = (j & 0xFF);
        }
        
        // Verify the pattern for a few earlier allocations
        if (i > 0 && i % 10 == 0) {
            int index = std::uniform_int_distribution<int>{0, i-1}(engine);
            unsigned char* prev_memory = reinterpret_cast<unsigned char*>(allocations[index].first);
            size_t prev_size = allocations[index].second;
            
            // Check a few random locations
            for (int k = 0; k < 10; k++) {
                size_t offset = std::uniform_int_distribution<size_t>{0, prev_size-1}(engine);
                EXPECT_EQ(prev_memory[offset], (offset & 0xFF));
            }
        }
    }
    
    // Clean up all allocations
    for (const auto& pair : allocations) {
        dealloc_to_os(pair.first, pair.second);
    }
}

TEST(OsAllocator, EdgeCases) {
    // Test zero-size allocation
    void* zero_result = alloc_from_os(0);
    EXPECT_FALSE(zero_result);
    
    // Test very large allocation (may fail if not enough memory)
    // This is more of a performance test than a functionality test
    size_t large_size = 100 * 1024 * 1024; // 100MB
    void* large_result = alloc_from_os(large_size);
    if (large_result) {
        // If allocation succeeded, make sure we can write to it
        char* memory = reinterpret_cast<char*>(large_result);
        memory[0] = 1;
        memory[large_size/2] = 2;
        memory[large_size-1] = 3;
        
        EXPECT_EQ(memory[0], 1);
        EXPECT_EQ(memory[large_size/2], 2);
        EXPECT_EQ(memory[large_size-1], 3);
        
        dealloc_to_os(large_result, large_size);
    }
}
