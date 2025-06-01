# malloc: Dynamic Memory Allocator in C

This project implements a custom dynamic memory allocator in C that provides implementations of malloc, free, realloc, and calloc using segregated free lists and explicit free list pointers to manage free blocks. It ensures 16-byte aligned payloads, uses footer-based boundary tags for block metadata, and applies a first-fit search within size-segregated lists for allocation. The allocator performs heap consistency checks that verify free block markings, coalescing correctness, free list reachability, and absence of overlapping blocks. It relies on a simulated memory system (memlib) to model heap growth and memory operations. This project serves as a practical example of dynamic memory management and allocator design in C.

## Output

![s1](https://github.com/user-attachments/assets/5d847775-0737-475c-94eb-3152a11e62fd)


## Support routines

The `memlib.c` package simulates the memory system for the dynamic memory allocator. 

- [`memlib.h`](https://gist.github.com/geeksprogramming/7ecd3154964f30ccab408a941cbd3384)
- [`memlib.c`](https://gist.github.com/geeksprogramming/0f0a4333012c852bcb61fefab7ee5d5f)

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
