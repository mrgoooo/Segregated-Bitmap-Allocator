#include <gtest/gtest.h>
#include <random>
#include <set>

extern "C" {
#include "balloc.h"
}

TEST(UserAPI, BasicAllocation) {
    balloc_setup();
    void* result = alloc(8);
    ASSERT_TRUE(result) << "no need to continue the test if there is no memory";
    
    // Should be able to write to the memory
    int *ptr = reinterpret_cast<int *>(result);
    *ptr = 5;
    EXPECT_EQ(*ptr, 5);
    
    // Clean up
    dealloc(result);
    balloc_teardown();
}

TEST(UserAPI, TwoAllocations) {
    balloc_setup();
    size_t alloc_size = 16;
    void* first_result = alloc(alloc_size);
    ASSERT_TRUE(first_result);
    
    void* second_result = alloc(alloc_size);
    ASSERT_TRUE(second_result);
    
    // non-overlapping memory regions
    EXPECT_NE(first_result, second_result);
    
    // Assert that memory regions do not communicate
    int *fst_int = reinterpret_cast<int *>(first_result);
    int *sec_int = reinterpret_cast<int *>(second_result);
    
    fst_int[0] = 10;
    fst_int[1] = 20;
    fst_int[2] = 30;
    fst_int[3] = 40;
    
    sec_int[0] = 50;
    sec_int[1] = 60;
    sec_int[2] = 70;
    sec_int[3] = 80;
    
    EXPECT_EQ(fst_int[0], 10);
    EXPECT_EQ(fst_int[1], 20);
    EXPECT_EQ(fst_int[2], 30);
    EXPECT_EQ(fst_int[3], 40);
    
    EXPECT_EQ(sec_int[0], 50);
    EXPECT_EQ(sec_int[1], 60);
    EXPECT_EQ(sec_int[2], 70);
    EXPECT_EQ(sec_int[3], 80);
    
    // Clean up
    dealloc(first_result);
    dealloc(second_result);
    balloc_teardown();
}

TEST(UserAPI, VariousSizes) {
    balloc_setup();
    // Test different allocation sizes
    std::vector<size_t> sizes = {
        1,                  // Tiny allocation
        8,                  // Small allocation
        16,                 // Small allocation
        32,                 // Small allocation
        64,                 // Medium allocation
        128,                // Medium allocation
        256,                // Medium allocation
        512,                // Medium allocation
        1024,               // Large allocation
        4096,               // Page-sized allocation
        10000,              // Odd-sized allocation
    };
    
    std::vector<void*> allocations;
    
    for (size_t size : sizes) {
        void* result = alloc(size);
        ASSERT_TRUE(result) << "Failed to allocate " << size << " bytes";
        allocations.push_back(result);
        
        // Write to the memory to ensure it's usable
        unsigned char* memory = reinterpret_cast<unsigned char*>(result);
        for (size_t i = 0; i < size; i++) {
            memory[i] = i & 0xFF;
        }
        
        // Check a subset of the written values
        for (size_t i = 0; i < size; i += (size / 10) + 1) {
            EXPECT_EQ(memory[i], i & 0xFF);
        }
    }
    
    // Clean up all allocations
    for (void* ptr : allocations) {
        dealloc(ptr);
    }
    balloc_teardown();
}

TEST(UserAPI, AllocationReuse) {
    balloc_setup();
    // Allocate memory of size 16
    void* mem1 = alloc(16);
    ASSERT_TRUE(mem1);
    
    // Write a pattern
    int* ptr1 = reinterpret_cast<int*>(mem1);
    ptr1[0] = 0xdeadbeef;
    ptr1[1] = 0xcafebabe;
    ptr1[2] = 0xfeedface;
    ptr1[3] = 0xbaadf00d;
    
    // Free the memory
    dealloc(mem1);
    
    // Allocate again with the same size
    void* mem2 = alloc(16);
    ASSERT_TRUE(mem2);
    
    // In an efficient allocator, we should get the same memory block back
    // Our bitmap allocator should definitely reuse blocks of the same size
    EXPECT_EQ(mem1, mem2);
    
    // The memory might still contain the old pattern
    // (not guaranteed, but likely for our implementation)
    int* ptr2 = reinterpret_cast<int*>(mem2);
    
    // Write a new pattern
    ptr2[0] = 0x12345678;
    ptr2[1] = 0x87654321;
    ptr2[2] = 0xabcdef01;
    ptr2[3] = 0x10fedcba;
    
    EXPECT_EQ(ptr2[0], 0x12345678);
    EXPECT_EQ(ptr2[1], 0x87654321);
    EXPECT_EQ(ptr2[2], 0xabcdef01);
    EXPECT_EQ(ptr2[3], 0x10fedcba);
    
    // Clean up
    dealloc(mem2);
    balloc_teardown();
}

TEST(UserAPI, ManyAllocations) {
    balloc_setup();
    const int num_allocations = 1000;
    std::vector<void*> allocations;
    
    // First phase: allocate many blocks
    for (int i = 0; i < num_allocations; i++) {
        size_t size = (i % 100) + BALLOC_ALIGNMENT; // Sizes 8-108
        // std::cout << "test" << i << "\n";
        void* result = alloc(size);
        ASSERT_TRUE(result);
        allocations.push_back(result);
        
        // Write a tag to identify this allocation
        *reinterpret_cast<int*>(result) = i;
        EXPECT_EQ(*reinterpret_cast<int*>(allocations[i]), i);
        // TODO: Remove
        for (int j = 0; j < i; j++) {
            EXPECT_EQ(*reinterpret_cast<int*>(allocations[j]), j);
        }
    }

    // Second phase: verify all allocations
    for (int i = 0; i < num_allocations; i++) {
        EXPECT_EQ(*reinterpret_cast<int*>(allocations[i]), i);
    }
    
    // Third phase: free half the allocations (evens)
    for (int i = 0; i < num_allocations; i += 2) {
        dealloc(allocations[i]);
        allocations[i] = nullptr;
    }
    
    // Fourth phase: verify remaining allocations
    for (int i = 1; i < num_allocations; i += 2) {
        EXPECT_EQ(*reinterpret_cast<int*>(allocations[i]), i);
    }
    
    // Fifth phase: reallocate the freed blocks with different sizes
    for (int i = 0; i < num_allocations; i += 2) {
        size_t size = ((i % 100) + 51); // Different size than before
        void* result = alloc(size);
        ASSERT_TRUE(result);
        allocations[i] = result;
        
        // Write a new tag
        *reinterpret_cast<int*>(result) = i + num_allocations;
    }
    
    // Sixth phase: verify all allocations again
    for (int i = 0; i < num_allocations; i++) {
        if (i % 2 == 0) {
            EXPECT_EQ(*reinterpret_cast<int*>(allocations[i]), i + num_allocations);
        } else {
            EXPECT_EQ(*reinterpret_cast<int*>(allocations[i]), i);
        }
    }
    
    // Final phase: free all allocations
    for (void* ptr : allocations) {
        if (ptr) dealloc(ptr);
    }
    balloc_teardown();
}

struct singly_linked_list {
    singly_linked_list *next;
};

TEST(UserAPI, MoreAllocations) {
    balloc_setup();
    const int num_allocations = 2 * 1024 * 1024;

    long sum = 0;
    singly_linked_list first {nullptr};
    for (singly_linked_list *curr {&first}; sum < num_allocations; sum +=
     sizeof(singly_linked_list)) {
        curr->next = reinterpret_cast<singly_linked_list *>(alloc(sizeof(
        singly_linked_list)));
        if (!curr->next) {
            printf("allocator returned 0\n");
            exit(1);
        }
        curr       = curr->next;
        curr->next = nullptr;
    }
    for (singly_linked_list *curr {first.next}; curr;) {
        auto copy = curr;
        curr      = curr->next;
        dealloc(copy);
    }
    balloc_teardown();
}

TEST(UserAPI, EdgeCases) {
    balloc_setup();
    // Test zero-size allocation
    void* zero_result = alloc(0);
    EXPECT_FALSE(zero_result);
    
    // Test multiple deallocations
    void* mem = alloc(16);
    ASSERT_TRUE(mem);
    dealloc(mem);
    
    // Test NULL deallocation
    dealloc(nullptr); // Should not crash
    
    // Test allocator creation for various sizes
    std::set<size_t> unique_sizes;
    std::vector<void*> allocations;
  
    for (int i = 1; i <= 20; i++) {
        size_t size = i * 10; // 10, 20, 30, ..., 200
        if (unique_sizes.find(size) == unique_sizes.end()) {
            void* result = alloc(size);
            ASSERT_TRUE(result);
            allocations.push_back(result);
            unique_sizes.insert(size);
        }
    }
   
    // Clean up
    for (void* ptr : allocations) {
        dealloc(ptr);
    }
    balloc_teardown();
}
