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
