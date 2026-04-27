#include "balloc.h"

#include <stdio.h>
#include <string.h>

#define BITMAP_PAGE_SIZE 4096

char static_array[BITMAP_PAGE_SIZE * 2];

void do_something_with_block(int argc, char **argv, void *memory) {
    char *string = (char *)memory;
    char *copy   = string;
    for (int i = 1; i < argc; ++i) {
        char *argument = argv[i];
        while (*argument) *copy++ = *argument++;
        *copy++ = ' ';
    }
    *copy++ = '\0';
    printf("input string is: %s\n", string);
}

// This function can be modifed to test your implementation
int main(int argc, char **argv) {
    balloc_setup();

    int num_input_chars = argc + 1; // spaces and terminating \0
    for (int i = 0; i < argc; ++i) num_input_chars += strlen(argv[i]);

    void *memory;
    
    // Set up a bitmap allocator for our static array
    struct bitmap_alloc static_alloc = {
        .chunk_size = num_input_chars,
        .occupied_areas = 0,
        .memory = static_array
    };
    
    printf("Testing allocation in static bitmap:\n");
    memory = alloc_block_in_bitmap(&static_alloc);
    if (memory) {
        do_something_with_block(argc, argv, memory);
        dealloc_block_in_bitmap(&static_alloc, memory);
    } else {
        printf("Failed to allocate from static bitmap\n");
    }

    printf("\nTesting OS allocation:\n");
    memory = alloc_from_os(num_input_chars);
    if (memory) {
        do_something_with_block(argc, argv, memory);
        dealloc_to_os(memory, num_input_chars);
    } else {
        printf("Failed to allocate from OS\n");
    }

    printf("\nTesting general allocation:\n");
    memory = alloc(num_input_chars);
    if (memory) {
        do_something_with_block(argc, argv, memory);
        dealloc(memory);
    } else {
        printf("Failed to allocate memory\n");
    }
    balloc_teardown();
    
    return 0;
}
