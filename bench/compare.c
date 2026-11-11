#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "libmemctl.h"

#define N 50000
#define ROUNDS 5

typedef void *(*alloc_fn)(size_t);
typedef void (*free_fn)(void *);

static volatile unsigned char sink;

double benchmark(alloc_fn allocate, free_fn release) {
    void *ptrs[N];
    size_t sizes[N];

    for (int i = 0; i < N; i++)
        sizes[i] = 16 + (i % 64) * 16;

    double total = 0;

    for (int r = 0; r < ROUNDS; r++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < N; i++) {
            ptrs[i] = allocate(sizes[i]);
            if (!ptrs[i]) {
                fprintf(stderr, "Allocation failed\n");
                exit(1);
            }
            ((unsigned char *)ptrs[i])[0] = (unsigned char)i;
            sink ^= ((unsigned char *)ptrs[i])[0];
        }

        for (int i = 0; i < N; i++)
            release(ptrs[i]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        double elapsed = (end.tv_sec - start.tv_sec) * 1e9
                       + (end.tv_nsec - start.tv_nsec);
        total += elapsed;
    }

    return total / (ROUNDS * 2.0 * N);
}

int main(void) {
    double system_time = benchmark(malloc, free);
    double custom_time = benchmark(my_malloc, my_free);

    printf("System malloc/free: %.2f ns per operation\n", system_time);
    printf("libmemctl malloc/free: %.2f ns per operation\n", custom_time);

    if (custom_time < system_time)
        printf("libmemctl is %.2fx faster in this test\n",
               system_time / custom_time);
    else
        printf("System allocator is %.2fx faster in this test\n",
               custom_time / system_time);

    return 0;
}
