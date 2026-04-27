#include <cstddef>
#include <gtest/gtest.h>

#include <array>

extern "C" {
#include "balloc.h"
}

struct GetFromBitmap : ::testing::Test {
    std::array<std::byte, 64 * sizeof(size_t)> backing_storage;
    struct bitmap_alloc alloc;

    GetFromBitmap() : backing_storage{} {
        alloc.chunk_size = sizeof(size_t);
        alloc.occupied_areas = 0;
        alloc.memory = backing_storage.data();
    }
};

TEST_F(GetFromBitmap, BasicAllocation) {
    void* result = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(result) << "no need to continue the test if there is no memory";
    
    EXPECT_GE(reinterpret_cast<char*>(result), reinterpret_cast<char*>(this->alloc.memory));
    EXPECT_LT(reinterpret_cast<char*>(result),
        reinterpret_cast<char*>(this->alloc.memory) + (sizeof(size_t) * 8 * this->alloc.chunk_size));

    // Check that the bitmap was updated
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1);

    int *ptr = reinterpret_cast<int *>(result);
    *ptr = 5; // if you give illegal memory, this should crash
    EXPECT_EQ(*ptr, 5);
}

TEST_F(GetFromBitmap, TwoAllocations) {
    void* first_result = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(first_result);
    
    void* second_result = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(second_result);
    
    // non-overlapping memory regions
    EXPECT_NE(first_result, second_result);
    char *fst_ptr = reinterpret_cast<char *>(first_result);
    char *sec_ptr = reinterpret_cast<char *>(second_result);
    
    // Chunks should be alloc.chunk_size apart
    EXPECT_EQ(std::abs(fst_ptr - sec_ptr), this->alloc.chunk_size);
    
    // Check that the bitmap was updated
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)3); // Binary 11

    // Assert that memory regions do not communicate
    *fst_ptr = 8;
    EXPECT_EQ(*fst_ptr, 8);
    *sec_ptr = 9;
    EXPECT_EQ(*sec_ptr, 9);
    EXPECT_EQ(*fst_ptr, 8); // old value
    *fst_ptr = 10;
    EXPECT_EQ(*sec_ptr, 9);
}

TEST_F(GetFromBitmap, AllocationExhaustion) {
    // Allocate all available chunks
    std::vector<void*> allocations;
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        void* result = alloc_block_in_bitmap(&this->alloc);
        ASSERT_TRUE(result);
        allocations.push_back(result);
        
        // Verify each allocation
        EXPECT_GE(reinterpret_cast<char*>(result), reinterpret_cast<char*>(this->alloc.memory));
        EXPECT_LT(reinterpret_cast<char*>(result),
            reinterpret_cast<char*>(this->alloc.memory) + (sizeof(size_t) * 8 * this->alloc.chunk_size));
    }
    
    // Try one more allocation, should fail
    void* one_too_many = alloc_block_in_bitmap(&this->alloc);
    EXPECT_FALSE(one_too_many);
    
    // Check that the bitmap is full
    EXPECT_EQ(this->alloc.occupied_areas, ~(size_t)0);
}

TEST_F(GetFromBitmap, ManyAllocations) {
    // Allocate half the available chunks
    const size_t num_allocations = sizeof(size_t) * 4;
    std::vector<void*> allocations;
    
    for (size_t i = 0; i < num_allocations; i++) {
        void* result = alloc_block_in_bitmap(&this->alloc);
        ASSERT_TRUE(result);
        allocations.push_back(result);
        
        // Write to the memory
        *reinterpret_cast<size_t*>(result) = i;
    }
    
    // Verify all allocations have the correct values
    for (size_t i = 0; i < num_allocations; i++) {
        EXPECT_EQ(*reinterpret_cast<size_t*>(allocations[i]), i);
    }
    
    // Check that the bitmap has the right number of bits set
    EXPECT_EQ(__builtin_popcountl(this->alloc.occupied_areas), num_allocations);
}
