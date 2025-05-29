# malloc: Dynamic Memory Allocator in C

A custom dynamic memory allocator in C that mimics the behavior of malloc, free, realloc, and calloc with a segregated free list and explicit free list management. This allocator features 16-byte aligned payloads, footer optimization, and heap consistency checking for robust debugging.

## Features

- Segregated Free List  
- Explicit Free List Management  
- First Fit Allocation Strategy  
- 16-byte Aligned Payloads  
- Footer Optimization  
- Heap Consistency Checking  

## Description of the Dynamic Memory Allocator Functions

- **`mm_init`**  
  Before calling `malloc`, `realloc`, `calloc`, or `free`, `mm_init` must be invoked to perform any necessary initializations, such as allocating the initial heap area.

- **`malloc`**  
  The `malloc` function returns a pointer to an allocated block payload of at least `size` bytes. The entire allocated block lies within the heap region and does not overlap with any other allocated chunk. If out of space and `mm_sbrk` is unable to extend the heap, it returns `NULL`. All returned pointers are 16-byte aligned, matching the standard behavior of the C library.

- **`free`**  
  The `free` function frees the block pointed to by `ptr`. It returns nothing. The function is valid only when the pointer was returned by an earlier call to `malloc`, `calloc`, or `realloc` and has not already been freed. If `ptr` is `NULL`, `free` does nothing.

- **`realloc`**  
  The `realloc` function returns a pointer to an allocated region of at least `size` bytes with the following constraints:

  - If `ptr` is `NULL`, the call is equivalent to `malloc(size)`.
  - If `size` is zero, the call is equivalent to `free(ptr)` and `NULL` is returned.
  - If `ptr` is not `NULL`, it must have been returned by an earlier call to `malloc`, `calloc`, or `realloc`. The call changes the size of the memory block pointed to by `ptr` to `size` bytes and returns the address of the new block. The address might be the same or different, depending on internal fragmentation and requested size.

  The contents of the new block are the same as those of the old `ptr` block, up to the minimum of the old and new sizes. Everything else is uninitialized. For example, if the old block is 8 bytes and the new block is 12 bytes, the first 8 bytes of the new block are identical to the old, and the last 4 are uninitialized. Similarly, if the old block is 8 bytes and the new block is 4 bytes, the new block contains the first 4 bytes of the old block.

## Support routines

The `memlib.c` package simulates the memory system for the dynamic memory allocator. 

- [`memlib.h`](https://gist.github.com/geeksprogramming/7ecd3154964f30ccab408a941cbd3384)
- [`memlib.c`](https://gist.github.com/geeksprogramming/0f0a4333012c852bcb61fefab7ee5d5f)

The following functions are available in `memlib.c`:

- `void* mm_sbrk(int incr)`: Expands the heap by `incr` bytes, where `incr` is a positive non-zero integer. It returns a generic pointer to the first byte of the newly allocated heap area. The semantics are identical to the Unix `sbrk` function, but `mm_sbrk` accepts only non-negative integer arguments. Use `mm_sbrk` for the tests; do **not** use `sbrk`.

- `void* mm_heap_lo(void)`: Returns a generic pointer to the first byte in the heap.

- `void* mm_heap_hi(void)`: Returns a generic pointer to the last byte in the heap.

- `size_t mm_heapsize(void)`: Returns the current size of the heap in bytes.

- `size_t mm_pagesize(void)`: Returns the system's page size in bytes (4K on Linux systems).

- `void* memset(void* ptr, int value, size_t n)`: Sets the first `n` bytes of memory pointed to by `ptr` to `value`.

- `void* memcpy(void* dst, const void* src, size_t n)`: Copies `n` bytes from `src` to `dst`.

## File Structure

- `mm.c` – Core implementation file where allocator logic resides

## Heap Consistency Checker

The `mm_checkheap()` function validates internal invariants such as:

- All blocks in the free list are marked free
- No consecutive free blocks are uncoalesced
- All free blocks are reachable from the free list
- No pointer overflows or invalid addresses
- No overlapping allocated blocks

## Example Usage

```c
mm_init(); // Initialize the heap

void* p1 = malloc(32);
void* p2 = malloc(64);

free(p1);

void* p3 = realloc(p2, 128);

mm_checkheap(__LINE__); // Optional: check heap health
```

---

This project demonstrates a hands-on understanding of low-level memory management, pointer arithmetic, and system-level C programming.
