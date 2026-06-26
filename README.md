# Bitmap Allocator

A custom memory allocator written in C using a bitmap-based memory management system and slab allocation technique.

## Description

This project implements a custom memory allocator similar to `malloc()` and `free()`.  
The allocator manages memory manually by requesting memory from the operating system using `mmap()` and dividing it into fixed-size memory blocks.

Each memory block size has its own bitmap that tracks which blocks are currently allocated.

Supported slab sizes:

8, 16, 32, 64, 128, 256,  
512, 1024, 2048, 4096,  
8192, 16384 bytes

If the requested allocation size does not match an existing slab, the allocator creates a new memory region dynamically.

## Features

- Custom memory allocation using `alloc()`
- Custom memory deallocation using `dealloc()`
- Bitmap-based memory tracking
- Slab allocation strategy
- Memory management using `mmap()` and `munmap()`
- Fast allocation using bit operations
- Dynamic creation of new memory regions

## Implementation

Each bitmap allocator stores:

- `chunk_size` - size of a single memory block
- `memory` - managed memory area
- `occupied_areas` - bitmap representing allocated blocks

Example bitmap:

```
[0][1][0][1][0][0][1][0]

0 - free block
1 - allocated block
```

During allocation, the allocator searches for the first free block and marks it as occupied.

During deallocation, the corresponding bit is cleared and the block becomes available again.

## Example Usage

```c
#include "balloc.h"

int main()
{
    balloc_setup();

    int *data = alloc(sizeof(int));

    *data = 42;

    dealloc(data);

    balloc_teardown();

    return 0;
}
```

## Project Structure

```
.
├── balloc.c
├── balloc.h
├── tests/
└── README.md
```

## Technologies

- C programming language
- Linux system calls (`mmap`, `munmap`)
- Bitmap memory allocation
- Slab allocation technique

## Purpose

The goal of this project is to demonstrate low-level memory management concepts and implement a custom memory allocator without using the standard C library allocation functions.
## Prerequisites

You need `cmake` and a working C/C++ compiler.

## Workflow

Build sample program (creates `BitmapAlloc` and `BitmapAllocOpt` executables in `build`):
```
cmake -B build && cmake --build build -j
```

This command also builds the test cases (in `build/tests`) and benchmarks (in `build/bench`)

Run tests by executing the according executables,
or by executing this `bash`-onliner:
```
find build/tests/* -prune -type f -executable '(' -exec {} ';' -or -quit ')'
```
