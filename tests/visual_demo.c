#include "libmemctl.h"
#include <stdio.h>

// Declare the function from allocator.c so we can call it
void debug_print_heap_map(void);

int main() {
    printf("--- Allocating 10 blocks of 32 bytes ---\n");
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = my_malloc(32);
    }

    printf("--- Freeing 5 blocks ---\n");
    for (int i = 0; i < 5; i++) {
        my_free(ptrs[i]);
    }

    // Call the visualizer!
    debug_print_heap_map();

    printf("--- Freeing remaining 5 blocks ---\n");
    for (int i = 5; i < 10; i++) {
        my_free(ptrs[i]);
    }
    
    return 0;
}
