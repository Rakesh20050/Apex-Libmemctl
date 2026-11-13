# Apex-Libmemctl

A high-throughput, thread-safe memory allocator implemented in C11.

## Features

- Custom allocation using `my_malloc()`.
- Memory deallocation using `my_free()`.
- Memory resizing using `my_realloc()`.
- Zero-initialized allocation using `my_calloc()`.
- Small allocations managed using size classes and free lists.
- Large allocations handled using `mmap()`.
- Thread safety using POSIX pthread mutexes.
- Memory alignment and allocation overflow checks.

## Requirements

- Linux
- GCC
- Make
- POSIX pthread support

## Build

Run:

    make

## Run Tests

    make test

## Run All Checks

    make check

## Check Allocator Symbols

    make check-symbols

## Benchmark

Compile:

    cc -D_GNU_SOURCE -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude bench/compare.c src/allocator.c -pthread -o build/compare

Run:

    ./build/compare

## Project Structure

- `src/` - Allocator implementation.
- `include/` - Public header files.
- `tests/` - Test suite.
- `bench/` - Benchmark programs.
- `DESIGN.md` - Allocator design documentation.
- `BENCHMARKS.md` - Benchmark results.


## Novelty & Innovation Features

To go beyond a standard allocator, we implemented five  specific engineering optimizations:

1. 'In-Place `realloc` (Performance)'
   When resizing a block, if the new size fits within the block's current size class capacity (and is at least half the capacity), the allocator returns the exact same pointer. This avoids costly memory copying and lock contention.

2. 'Hardened Debug Mode (Robustness & Security)'
   When compiled with `-DMEMCTL_DEBUG`, the allocator injects magic numbers (`0xA110C8ED`) into block headers, poisons freed memory with `0xDD`, and actively detects double-frees or invalid pointers, printing a warning to the terminal instead of crashing.

3. 'calloc` Zero-Skip for Large Allocations (Throughput)'
   Since the Linux kernel guarantees that fresh `mmap()` pages are zeroed, our allocator skips the manual `memset()` for large blocks (>32KB) that come directly from the OS. We only zero recycled blocks, saving millions of CPU cycles.

4. 'Thread Caches with Exit-Flush (Scalability)'
   Each thread maintains a private cache of up to 32 blocks per size class. Threads allocate and free memory with 'zero locks' by pulling from their local stash. A `pthread_key` destructor ensures cached blocks are safely returned to the global pool when a thread exits, achieving massive multi-threaded throughput (~5.3 million ops/sec).

5. Heap Visualizer (Demonstration & Diagnostics)
   We added an ASCII heap visualizer (`debug_print_heap_map`) under the `MEMCTL_DEBUG` flag. It prints the state of the global free lists. Because of our Thread Caches, the global heap map often appears empty during high-throughput operations, proving that threads are successfully bypassing the global locks to allocate and free memory locally.


### Heap Visualizer (Novelty 5)

Compile:
```bash
gcc -DMEMCTL_DEBUG -Iinclude -c src/allocator.c -o build/allocator_debug.o
gcc -Iinclude tests/visual_demo.c build/allocator_debug.o -pthread -o build/visual_demo


./build/visual_demo
