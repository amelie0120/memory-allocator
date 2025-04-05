# Memory Allocator

## Overview

I built a minimalistic memory allocator library that provides custom implementations of `malloc()`, `calloc()`, `realloc()`, and `free()`. This project helped me:

- Learn the fundamentals of memory management
- Work directly with Linux memory management syscalls: `brk()`, `mmap()`, and `munmap()`
- Understand and address the bottlenecks of memory allocation

The resulting library (`libosmem.so`) is a reliable solution for explicit allocation, reallocation, and initialization of memory.

## Features Implemented

### Memory Management Functions

1. **`void *os_malloc(size_t size)`**
   - Allocates specified bytes and returns a pointer to the allocated memory
   - Uses `brk()` for small allocations and `mmap()` for larger ones (threshold-based approach)
   - Supports proper memory alignment for 64-bit systems (8 bytes)

2. **`void *os_calloc(size_t nmemb, size_t size)`**
   - Allocates memory for an array of elements and initializes to zero
   - Utilizes `brk()` for allocations smaller than page size and `mmap()` for larger ones

3. **`void *os_realloc(void *ptr, size_t size)`**
   - Changes the size of existing memory blocks
   - Implements smart expansion by coalescing adjacent free blocks when possible
   - Handles edge cases properly (NULL pointers, zero sizes)

4. **`void os_free(void *ptr)`**
   - Frees previously allocated memory
   - Correctly handles heap memory (marking as free for reuse) and mapped memory (calling `munmap()`)

### Advanced Memory Management Techniques

I implemented several optimization strategies to make the allocator efficient:

#### Memory Block Tracking
- Used a `struct block_meta` to manage metadata for each memory block
- Maintained a linked list of allocated blocks for tracking and reuse

#### Block Splitting
- Implemented block splitting to reduce internal fragmentation
- Ensured proper alignment of both metadata and payload

#### Block Coalescing
- Added block coalescing to combat external fragmentation by joining adjacent free blocks
- Applied coalescing before block searches and during reallocation

#### Best Fit Algorithm
- Implemented a "find best" strategy to select the most appropriate free block for a given allocation
- Searched the entire list to find the smallest suitable free block

#### Heap Preallocation
- Preallocated a large chunk (128 KB) on first heap use to reduce future syscalls
- Split this preallocated memory for subsequent small allocations

## Implementation Details

### Memory Alignment
All memory allocations are aligned to 8 bytes as required by 64-bit systems, providing efficient memory access patterns.

### Error Handling
Implemented robust error handling for all syscalls to ensure the library functions reliably under various conditions.

## Testing

The implementation passed a comprehensive test suite covering:
- Basic allocations and deallocations
- Edge cases (zero-sized allocations, NULL pointers)
- Block reuse scenarios
- Block splitting and coalescing
- Various allocation patterns and sequences


## Resources

- ["Implementing malloc" slides by Michael Saelee](https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf)
- [Malloc Tutorial](https://danluu.com/malloc-tutorial/)
