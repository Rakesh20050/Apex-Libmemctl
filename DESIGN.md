
# libmemctl Design

## 1. Overview
libmemctl is a custom memory allocator implemented in C.

It provides four public functions:
- my_malloc()
- my_free()
- my_calloc()
- my_realloc()

## 2. Memory Management
The allocator uses mmap() to obtain memory from the operating system.

For small allocations, it creates memory arenas of 1 MB.
Each arena is divided into blocks based on predefined size classes.

## 3. Size Classes and Free Lists
The allocator supports 40 size classes, ranging from 16 bytes to 32 KB.

Freed blocks are stored in free lists and reused for future allocations.

## 4. Large Allocations
Requests larger than 32 KB use a separate mmap() allocation.

When freed, these allocations are returned using munmap().

## 5. Thread Safety
The allocator uses pthread mutexes.

Each size class has its own lock to protect its free list and reduce lock contention between different size classes.

## 6. Reallocation
The realloc implementation supports in-place resizing for eligible small allocations.

If an allocation cannot be resized in place, a new block is allocated and the existing data is copied.

## 7. Debugging
Optional debug mode uses magic values to identify allocated and freed blocks.

Freed memory is filled with a poison pattern to help detect memory misuse.

## 8. Zero Initialization
calloc checks for multiplication overflow before allocating memory.

Small allocations are explicitly zeroed. Large allocations use the zero-filled memory provided by anonymous mmap().

