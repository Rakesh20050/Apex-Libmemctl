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

To go beyond a standard allocator, we implemented three specific engineering optimizations:

1. 'In-Place `realloc` (Performance)'
   When resizing a block, if the new size fits within the block's current size class capacity (and is at least half the capacity), the allocator returns the exact same pointer. This avoids costly memory copying and lock contention.

2. 'Hardened Debug Mode (Robustness & Security)'
   When compiled with `-DMEMCTL_DEBUG`, the allocator injects magic numbers (`0xA110C8ED`) into block headers, poisons freed memory with `0xDD`, and actively detects double-frees or invalid pointers, printing a warning to the terminal instead of crashing.

3. 'calloc` Zero-Skip for Large Allocations (Throughput)'
   Since the Linux kernel guarantees that fresh `mmap()` pages are zeroed, our allocator skips the manual `memset()` for large blocks (>32KB) that come directly from the OS. We only zero recycled blocks, saving millions of CPU cycles.
