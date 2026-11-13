#include "libmemctl.h"
#include <stdio.h>
#include <assert.h>

#define ALLOC_COUNT 1000

int main() {
    printf("--- Testing Arena Reuse (Person B's Fix) ---\n");
    
    void *ptrs[ALLOC_COUNT];
    
    // 1. Allocate 1000 blocks (forces an arena carve)
    for (int i = 0; i < ALLOC_COUNT; i++) {
        ptrs[i] = my_malloc(32); 
        assert(ptrs[i] != NULL);
    }
    printf("Allocated %d blocks.\n", ALLOC_COUNT);

    // 2. Free them all
    for (int i = 0; i < ALLOC_COUNT; i++) {
        my_free(ptrs[i]);
    }
    printf("Freed all blocks.\n");

    // 3. Allocate 1000 blocks again
    void *ptrs2[ALLOC_COUNT];
    for (int i = 0; i < ALLOC_COUNT; i++) {
        ptrs2[i] = my_malloc(32);
    }

    // 4. Check if they reused the EXACT same addresses
    int reused = 0;
    for (int i = 0; i < ALLOC_COUNT; i++) {
        for (int j = 0; j < ALLOC_COUNT; j++) {
            if (ptrs2[i] == ptrs[j]) {
                reused++;
                break;
            }
        }
    }

    printf("Reused %d out of %d blocks.\n", reused, ALLOC_COUNT);
    if (reused >= (ALLOC_COUNT - 64)) { // Allowing 64 for thread cache variance
        printf("SUCCESS: Arena memory is fully reusable!\n");
    } else {
        printf("FAIL: Memory was leaked or not reused.\n");
        return 1;
    }

    return 0;
}
