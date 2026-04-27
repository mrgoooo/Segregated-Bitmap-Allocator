#include <benchmark/benchmark.h>
#include <cstddef>
#include <vector>
#include <random>

extern "C" {
#include "balloc.h"
}

// ===== EXISTING BENCHMARKS (for reference) =====

// Equivalent to zero_byte_flexible
static void BM_ZeroByteAlloc(benchmark::State &state) {
    balloc_setup();
    for (auto _ : state) {
        auto inner = alloc(0);
        dealloc(inner);
    }
    balloc_teardown();
}
BENCHMARK(BM_ZeroByteAlloc);

// Equivalent to one_byte_flexible
static void BM_OneByteAlloc(benchmark::State &state) {
    balloc_setup();
    for (auto _ : state) {
        auto inner = alloc(1);
        dealloc(inner);
    }
    balloc_teardown();
}
BENCHMARK(BM_OneByteAlloc);

// Equivalent to many_bytes_flexible
static void BM_ManyBytesAlloc(benchmark::State &state) {
    constexpr long NUM_BYTES = 0b1111 << 10;
    balloc_setup();
    for (auto _ : state) {
        for (long i = 1; (1L << i) < NUM_BYTES; ++i) {
            size_t size = 1L << i | 1L << (i - 1);
            auto inner  = alloc(size);
            if (inner) {
                *reinterpret_cast<volatile char *>(inner) = 5;
                dealloc(inner);
            }
        }
    }
    balloc_teardown();
}
BENCHMARK(BM_ManyBytesAlloc);

struct singly_linked_list {
    singly_linked_list *next;
};

// Equivalent to linked_list_flexible
static void BM_LinkedListAlloc(benchmark::State &state) {
    constexpr auto limit = 4 * 1024 * 1024;
    balloc_setup();
    for (auto _ : state) {
        long sum = 0;
        singly_linked_list first {nullptr};
        for (singly_linked_list *curr {&first}; sum < limit; sum += sizeof(singly_linked_list)) {
            curr->next = reinterpret_cast<singly_linked_list *>(alloc(sizeof(singly_linked_list)));
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
    }
    balloc_teardown();
}
BENCHMARK(BM_LinkedListAlloc);


// ===== ALLOCATION PATTERN BENCHMARKS =====

// Benchmark for alternating allocation and deallocation
static void BM_AlternateAllocDealloc(benchmark::State &state) {
    const size_t size = state.range(0);
    balloc_setup();

    for (auto _ : state) {
        void *ptr = alloc(size);
        benchmark::DoNotOptimize(ptr);
        dealloc(ptr);
    }

    balloc_teardown();
}
BENCHMARK(BM_AlternateAllocDealloc)->Range(8, 4096);

// Benchmark for allocating multiple blocks, then deallocating them all
static void BM_BatchAllocDealloc(benchmark::State &state) {
    const int batch_size    = state.range(0);
    const size_t alloc_size = 32; // Fixed allocation size

    balloc_setup();

    std::vector<void *> allocations(batch_size);
    for (auto _ : state) {
        allocations.clear();

        // Allocate a batch
        for (int i = 0; i < batch_size; i++) {
            allocations[i] = alloc(alloc_size);
            benchmark::DoNotOptimize(allocations[i]);
        }

        // Deallocate the batch
        for (int i = 0; i < batch_size; i++) { dealloc(allocations[i]); }
    }

    balloc_teardown();
}
BENCHMARK(BM_BatchAllocDealloc)->Range(1, 1 << 10);

// Benchmark for allocation reuse (allocate-deallocate-allocate pattern)
static void BM_AllocationReuse(benchmark::State &state) {
    const int num_allocs    = state.range(0);
    const size_t alloc_size = 16; // Fixed allocation size

    balloc_setup();

    // Pre-allocate and free to setup allocators
    std::vector<void *> setup_allocs(num_allocs);
    for (int i = 0; i < num_allocs; i++) { setup_allocs[i] = alloc(alloc_size); }
    for (int i = 0; i < num_allocs; i++) { dealloc(setup_allocs[i]); }

    for (auto _ : state) {
        // Allocate blocks (should reuse previously setup blocks)
        std::vector<void *> allocations(num_allocs);
        for (int i = 0; i < num_allocs; i++) {
            allocations[i] = alloc(alloc_size);
            benchmark::DoNotOptimize(allocations[i]);
        }

        // Free all blocks
        for (int i = 0; i < num_allocs; i++) { dealloc(allocations[i]); }
    }

    balloc_teardown();
}
BENCHMARK(BM_AllocationReuse)->Range(1, 1 << 10);

// ===== SIZE CLASS BENCHMARKS =====

// Benchmark allocation by power-of-2 sizes
static void BM_PowerOf2Alloc(benchmark::State &state) {
    balloc_setup();

    for (auto _ : state) {
        std::vector<void *> allocations;
        for (int i = 3; i <= 12; i++) { // 2^3 to 2^12 bytes
            size_t size = 1 << i;
            void *ptr   = alloc(size);
            benchmark::DoNotOptimize(ptr);
            allocations.push_back(ptr);
        }

        for (void *ptr : allocations) { dealloc(ptr); }
    }

    balloc_teardown();
}
BENCHMARK(BM_PowerOf2Alloc);

// Benchmark allocation by non-power-of-2 sizes
static void BM_NonPowerOf2Alloc(benchmark::State &state) {
    balloc_setup();

    for (auto _ : state) {
        std::vector<void *> allocations;
        for (int i = 1; i <= 10; i++) {
            size_t size = (1 << i) + (1 << (i - 1)); // 1.5 times power of 2
            void *ptr   = alloc(size);
            benchmark::DoNotOptimize(ptr);
            allocations.push_back(ptr);
        }

        for (void *ptr : allocations) { dealloc(ptr); }
    }

    balloc_teardown();
}
BENCHMARK(BM_NonPowerOf2Alloc);

// ===== MIXED WORKLOAD BENCHMARKS =====

// Benchmark for random allocation/deallocation pattern
static void BM_RandomAllocDealloc(benchmark::State &state) {
    const int operations = state.range(0);
    std::mt19937 rng(42); // Fixed seed for reproducibility

    balloc_setup();

    std::vector<void *> active_allocations;
    for (auto _ : state) {
        active_allocations.clear();

        for (int i = 0; i < operations; i++) {
            // 70% chance to allocate, 30% chance to free
            bool should_allocate = (std::uniform_int_distribution<size_t> {1, 10}(rng) <= 7) ||
                                   active_allocations.empty();

            if (should_allocate) {
                // Choose size between 8 and 2048 bytes
                size_t size = std::uniform_int_distribution<size_t> {8, 2048}(rng);
                void *ptr   = alloc(size);
                benchmark::DoNotOptimize(ptr);
                active_allocations.push_back(ptr);
            } else {
                // Free a random allocation
                size_t index = std::uniform_int_distribution<size_t> {0, active_allocations.size() - 1}(rng);
                dealloc(active_allocations[index]);
                active_allocations.erase(active_allocations.begin() + index);
            }
        }

        // Clean up any remaining allocations
        for (void *ptr : active_allocations) { dealloc(ptr); }
    }

    balloc_teardown();
}
BENCHMARK(BM_RandomAllocDealloc)->Range(100, 1000);

// Benchmark for fragmentation scenario (interleaved small/large allocs and deallocs)
static void BM_Fragmentation(benchmark::State &state) {
    const int small_count = state.range(0);
    const int large_count = small_count / 10; // 10:1 ratio of small to large

    const size_t small_size = 16;
    const size_t large_size = 256;

    balloc_setup();

    std::vector<void *> small_allocs;
    std::vector<void *> large_allocs;
    for (auto _ : state) {
        small_allocs.clear();
        large_allocs.clear();

        // Allocate small objects
        for (int i = 0; i < small_count; i++) {
            void *ptr = alloc(small_size);
            benchmark::DoNotOptimize(ptr);
            small_allocs.push_back(ptr);
        }

        // Free every other small object
        for (int i = 0; i < small_count; i += 2) {
            dealloc(small_allocs[i]);
            small_allocs[i] = nullptr;
        }

        // Allocate large objects
        for (int i = 0; i < large_count; i++) {
            void *ptr = alloc(large_size);
            benchmark::DoNotOptimize(ptr);
            large_allocs.push_back(ptr);
        }

        // Allocate small objects again (should reuse free small slots)
        for (int i = 0; i < small_count; i += 2) {
            small_allocs[i] = alloc(small_size);
            benchmark::DoNotOptimize(small_allocs[i]);
        }

        // Free everything
        for (void *ptr : small_allocs) {
            if (ptr) dealloc(ptr);
        }
        for (void *ptr : large_allocs) { dealloc(ptr); }
    }

    balloc_teardown();
}
BENCHMARK(BM_Fragmentation)->Range(100, 1000);

// ===== REAL-WORLD SIMULATION BENCHMARKS =====

// Simulate a sequence of allocations/deallocations resembling a typical program pattern
static void BM_RealWorldSimulation(benchmark::State &state) {
    const int total_ops = state.range(0);
    std::mt19937 rng(42);

    // Define different allocation sizes to simulate different objects
    const std::vector<size_t> small_obj_sizes  = {8, 16, 24, 32};
    const std::vector<size_t> medium_obj_sizes = {64, 96, 128, 192, 256};
    const std::vector<size_t> large_obj_sizes  = {512, 1024, 2048, 4096};

    balloc_setup();

    std::vector<void *> small_objects;
    std::vector<void *> medium_objects;
    std::vector<void *> large_objects;
    for (auto _ : state) {
        small_objects.clear();
        medium_objects.clear();
        large_objects.clear();

        // Simulating program initialization - allocate mostly small objects
        int init_small  = total_ops / 5;
        int init_medium = total_ops / 20;

        for (int i = 0; i < init_small; i++) {
            size_t size =
                small_obj_sizes[std::uniform_int_distribution<size_t> {0, small_obj_sizes.size() - 1}(rng)];
            void *ptr = alloc(size);
            benchmark::DoNotOptimize(ptr);
            small_objects.push_back(ptr);
        }

        for (int i = 0; i < init_medium; i++) {
            size_t size =
                medium_obj_sizes[std::uniform_int_distribution<size_t> {0, medium_obj_sizes.size() - 1}(rng)];
            void *ptr = alloc(size);
            benchmark::DoNotOptimize(ptr);
            medium_objects.push_back(ptr);
        }

        // Simulating program steady state - mix of alloc/dealloc
        for (int i = 0; i < total_ops / 2; i++) {
            int action = std::uniform_int_distribution<int> {0, 9}(rng);

            if (action < 3 && !small_objects.empty()) {
                // Free a small object
                size_t idx = std::uniform_int_distribution<size_t> {0, small_objects.size() - 1}(rng);
                dealloc(small_objects[idx]);
                small_objects.erase(small_objects.begin() + idx);
            } else if (action < 5 && !medium_objects.empty()) {
                // Free a medium object
                size_t idx = std::uniform_int_distribution<size_t> {0, medium_objects.size() - 1}(rng);
                dealloc(medium_objects[idx]);
                medium_objects.erase(medium_objects.begin() + idx);
            } else if (action < 6 && !large_objects.empty()) {
                // Free a large object
                size_t idx = std::uniform_int_distribution<size_t> {0, large_objects.size() - 1}(rng);
                dealloc(large_objects[idx]);
                large_objects.erase(large_objects.begin() + idx);
            } else if (action < 8) {
                // Allocate a small object
                size_t size =
                    small_obj_sizes[std::uniform_int_distribution<size_t> {0, small_obj_sizes.size() - 1}(rng)];
                void *ptr = alloc(size);
                benchmark::DoNotOptimize(ptr);
                small_objects.push_back(ptr);
            } else if (action < 9) {
                // Allocate a medium object
                size_t size =
                    medium_obj_sizes[std::uniform_int_distribution<size_t> {0, medium_obj_sizes.size() - 1}(rng)];
                void *ptr = alloc(size);
                benchmark::DoNotOptimize(ptr);
                medium_objects.push_back(ptr);
            } else {
                // Allocate a large object
                size_t size =
                    large_obj_sizes[std::uniform_int_distribution<size_t> {0, large_obj_sizes.size() - 1}(rng)];
                void *ptr = alloc(size);
                benchmark::DoNotOptimize(ptr);
                large_objects.push_back(ptr);
            }
        }

        // Simulating program cleanup - free everything
        for (void *ptr : small_objects) { dealloc(ptr); }
        for (void *ptr : medium_objects) { dealloc(ptr); }
        for (void *ptr : large_objects) { dealloc(ptr); }
    }

    balloc_teardown();
}
BENCHMARK(BM_RealWorldSimulation)->Arg(100)->Arg(500)->Arg(1000);

// Benchmark traversing a tree-like structure with many small allocations
static void BM_TreeStructure(benchmark::State &state) {
    const int tree_depth        = state.range(0);
    const int children_per_node = 4;

    struct TreeNode {
        int value;
        TreeNode **children;
        int num_children;
    };

    auto create_node = [](int depth, int maxDepth) -> TreeNode * {
        TreeNode *node = (TreeNode *)alloc(sizeof(TreeNode));
        if (!node) return nullptr;

        node->value = depth;

        if (depth >= maxDepth) {
            node->children     = nullptr;
            node->num_children = 0;
            return node;
        }

        node->num_children = children_per_node;
        node->children     = (TreeNode **)alloc(sizeof(TreeNode *) * children_per_node);
        if (!node->children) {
            dealloc(node);
            return nullptr;
        }

        return node;
    };

    // Non-recursive tree deletion using a stack
    auto delete_tree = [](TreeNode *root) {
        if (!root) return;

        // Use vector as a stack for nodes to delete
        std::vector<TreeNode *> nodes_to_delete;
        nodes_to_delete.push_back(root);

        while (!nodes_to_delete.empty()) {
            TreeNode *current = nodes_to_delete.back();
            nodes_to_delete.pop_back();

            // Add children to the stack if they exist
            if (current->children) {
                for (int i = 0; i < current->num_children; i++) {
                    if (current->children[i]) { nodes_to_delete.push_back(current->children[i]); }
                }
                dealloc(current->children);
            }

            // Delete the current node
            dealloc(current);
        }
    };

    balloc_setup();

    std::vector<TreeNode *> next_level;
    for (auto _ : state) {
        // Create root node
        TreeNode *root = create_node(0, tree_depth);
        benchmark::DoNotOptimize(root);

        // Build tree in breadth-first manner
        std::vector<TreeNode *> current_level = {root};

        for (int depth = 1; depth <= tree_depth; depth++) {
            next_level.clear();

            for (TreeNode *parent : current_level) {
                if (!parent || !parent->children) continue;

                for (int i = 0; i < parent->num_children; i++) {
                    parent->children[i] = create_node(depth, tree_depth);
                    if (parent->children[i]) { next_level.push_back(parent->children[i]); }
                }
            }

            current_level = next_level;
        }

        // Cleanup
        delete_tree(root);
    }

    balloc_teardown();
}
BENCHMARK(BM_TreeStructure)->DenseRange(3, 7, 2);


static void BM_MixedMiddle(benchmark::State &state) {
    const int operations = state.range(0);
    std::mt19937 rng(1337); // Fixed seed for reproducibility

    balloc_setup();
    std::vector<void *> allocations;

    for (auto _ : state) {
        allocations.clear();
        for (int i = 0; i < operations; i++) {
            size_t size = std::uniform_int_distribution<size_t> {8, 8192}(rng);
            void *ptr   = alloc(size);
            benchmark::DoNotOptimize(ptr);
            allocations.push_back(ptr);
        }
        int mid  = operations / 2;
        int qrt  = operations / 4;
        int low  = mid - qrt;
        int high = mid + qrt;
        for (int i = mid + 1; i < high; i++) { dealloc(allocations[i]); }
        for (int i = mid; i >= low; i--) { dealloc(allocations[i]); }
        for (int i = high - 1; i >= low; i--) {
            size_t size = std::uniform_int_distribution<size_t> {8, 8192 * 8}(rng);
            void *ptr   = alloc(size);
            benchmark::DoNotOptimize(ptr);
            allocations[i] = ptr;
        }

        for (int i = mid; i >= 0; i--) { dealloc(allocations[i]); }
        for (int i = mid + 1; i < operations; i++) { dealloc(allocations[i]); }
    }
}

BENCHMARK(BM_MixedMiddle)->Range(1 << 8, 1 << 11);
BENCHMARK_MAIN();
