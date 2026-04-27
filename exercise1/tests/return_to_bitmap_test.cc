#include <cstddef>
#include <gtest/gtest.h>

#include <array>

extern "C" {
#include "balloc.h"
}

struct ReturnToBitmap : ::testing::Test {
    std::array<std::byte, 64 * sizeof(size_t)> backing_storage;
    struct bitmap_alloc alloc;

    ReturnToBitmap() : backing_storage{} {
        alloc.chunk_size = sizeof(size_t);
        alloc.occupied_areas = 0;
        alloc.memory = backing_storage.data();
    }
};

TEST_F(ReturnToBitmap, BasicDeallocation) {
    void* result = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(result) << "no need to continue the test if there is no memory";
    
    // Check that the bitmap was updated
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1);
    
    // Write to the memory
    *reinterpret_cast<int*>(result) = 42;
    
    // Free the memory
    dealloc_block_in_bitmap(&this->alloc, result);
    
    // Check that the bitmap is cleared
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0);
}

TEST_F(ReturnToBitmap, MultipleDeallocation) {
    // Allocate a few blocks
    void* first = alloc_block_in_bitmap(&this->alloc);
    void* second = alloc_block_in_bitmap(&this->alloc);
    void* third = alloc_block_in_bitmap(&this->alloc);
    
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    
    // Check bitmap state
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)7); // Binary 111
    
    // Write different values to each block
    *reinterpret_cast<int*>(first) = 1;
    *reinterpret_cast<int*>(second) = 2;
    *reinterpret_cast<int*>(third) = 3;
    
    // Free the second block
    dealloc_block_in_bitmap(&this->alloc, second);
    
    // Check bitmap state (only first and third should be allocated)
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)5); // Binary 101
    
    // First and third should still have their values
    EXPECT_EQ(*reinterpret_cast<int*>(first), 1);
    EXPECT_EQ(*reinterpret_cast<int*>(third), 3);
    
    // Allocate a new block - should get the freed second block
    void* reused = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(reused);
    
    // Should get the same address as second
    EXPECT_EQ(reused, second);
    
    // Write a new value
    *reinterpret_cast<int*>(reused) = 4;
    
    // Check that all values are correct
    EXPECT_EQ(*reinterpret_cast<int*>(first), 1);
    EXPECT_EQ(*reinterpret_cast<int*>(reused), 4);
    EXPECT_EQ(*reinterpret_cast<int*>(third), 3);
    
    // Free all blocks
    dealloc_block_in_bitmap(&this->alloc, first);
    dealloc_block_in_bitmap(&this->alloc, reused);
    dealloc_block_in_bitmap(&this->alloc, third);
    
    // Check that bitmap is cleared
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0);
}

TEST_F(ReturnToBitmap, ReusePattern) {
    // Allocate all available blocks
    std::vector<void*> blocks;
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        void* result = alloc_block_in_bitmap(&this->alloc);
        ASSERT_TRUE(result);
        blocks.push_back(result);
    }
    
    // Check that bitmap is full
    EXPECT_EQ(this->alloc.occupied_areas, ~(size_t)0);
    
    // Free blocks in same order
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
        dealloc_block_in_bitmap(&this->alloc, *it);
    }
    
    // Check that bitmap is empty
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0);
    
    // Reallocate all blocks
    std::vector<void*> new_blocks;
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        void* result = alloc_block_in_bitmap(&this->alloc);
        ASSERT_TRUE(result);
        new_blocks.push_back(result);
    }
    
    // Check allocation pattern - should be in same order due to bitmap allocation strategy
    // First bit found is allocated first
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        EXPECT_EQ(new_blocks[i], blocks[i]);
    }

    // Free blocks in reverse order
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
        dealloc_block_in_bitmap(&this->alloc, *it);
    }
    
    // Check that bitmap is empty
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0);
    
    // Reallocate all blocks
    std::vector<void*> reverse_blocks;
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        void* result = alloc_block_in_bitmap(&this->alloc);
        ASSERT_TRUE(result);
        reverse_blocks.push_back(result);
    }
    
    // Check allocation pattern - should be in same order due to bitmap allocation strategy
    // First bit found is allocated first
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        EXPECT_EQ(reverse_blocks[i], blocks[i]);
    }
}

TEST_F(ReturnToBitmap, InvalidDeallocations) {
    // Allocate a valid block
    void* valid = alloc_block_in_bitmap(&this->alloc);
    ASSERT_TRUE(valid);
    
    // Initial bitmap state
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1);
    
    // Test deallocating NULL
    dealloc_block_in_bitmap(&this->alloc, nullptr);
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1); // Should remain unchanged
    
    // Test deallocating address outside the memory region
    char* outside_ptr = reinterpret_cast<char*>(this->alloc.memory) + 
                        (sizeof(size_t) * 8 * this->alloc.chunk_size) + 10;
    dealloc_block_in_bitmap(&this->alloc, outside_ptr);
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1); // Should remain unchanged
    
    // Test deallocating unaligned address within a valid chunk
    char* unaligned_ptr = reinterpret_cast<char*>(valid) + 1;
    dealloc_block_in_bitmap(&this->alloc, unaligned_ptr);
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)1); // Should remain unchanged
    
    // Test deallocating already free block
    // First free the valid block
    dealloc_block_in_bitmap(&this->alloc, valid);
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0);
    
    // Try to free it again
    dealloc_block_in_bitmap(&this->alloc, valid);
    EXPECT_EQ(this->alloc.occupied_areas, (size_t)0); // Should remain unchanged
}
