
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

## 9. Novelty Implementations

Beyond the core arena architecture, we added five features to improve performance and robustness:

*  In-Place Realloc: Avoids `memcpy` if the requested size fits within the existing block's size class capacity.
*  Debug Hardening: Uses `#ifdef MEMCTL_DEBUG` to add canary magic numbers, poison freed memory (`0xDD`), and catch double-frees.
*  Zero-Skip Calloc: Skips `memset` on fresh large `mmap` blocks because the OS guarantees they are already zeroed.
*  Thread Caches (`_Thread_local`): Provides a zero-lock fast path for small allocations. Each thread keeps a private cache of blocks, flushing back to the global free list via a `pthread_key` destructor upon thread exit. This increased our 8-thread throughput from ~3.1M to ~5.3M ops/sec. 
*  Heap Visualizer: Implemented as a diagnostic function under `MEMCTL_DEBUG`. It iterates through the global free lists and prints an ASCII map of available blocks. This tool is used to demonstrate the effectiveness of Thread Caches: after freeing blocks, the global map remains empty because the blocks are safely stored in thread-local caches, avoiding global lock contention.
