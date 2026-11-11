# Benchmarks

## System
- OS: Ubuntu running in VMware
- Allocator: libmemctl
## System Call Results

Command:
strace -c -e trace=mmap,munmap ./build/test_runner

Results:
- mmap calls: 37
- munmap calls: 5
- Total system calls: 42

Baseline measurements: Completed.

System call measurements:
- mmap calls: 37
- munmap calls: 5
- Total system calls: 42

Allocator comparison:
- System malloc/free: 159.10 ns per operation
- libmemctl malloc/free: 149.33 ns per operation
- Relative performance: 1.07x faster in this test
## Post-fix validation

After the overflow-handling update:

- Functional tests: 6 passed, 0 failed
- mmap calls: 37
- munmap calls: 5
- Total mapping system calls: 42
- Threaded operations: 160000
- Threaded execution time: 0.037 seconds
- Threaded throughput: approximately 4.32 million operations/second

The system-call counts remained unchanged after the overflow fix.

## Updated comparison benchmark (after overflow fix)

- System malloc/free: 111.12 ns per operation
- libmemctl malloc/free: 70.52 ns per operation
- Measured ratio: 1.58x faster for libmemctl in this run
- Benchmark: 50,000 allocations and frees per round, 5 rounds
- Note: This is a simple microbenchmark result from one run, not a general performance guarantee.
