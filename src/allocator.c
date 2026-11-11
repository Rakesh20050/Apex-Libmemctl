#include "libmemctl.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define ARENA_SIZE (1024 * 1024)    // 1 MB Arenas
#define LARGE_THRESHOLD (32 * 1024) // Anything > 32KB is "Large"
#define NUM_CLASSES 40
#define LARGE_CLASS_ID 999

// Magic numbers for Debug Mode
#define MAGIC_LIVE  0xA110C8ED 
#define MAGIC_FREE  0xDEADF4EE 
#define MAGIC_LARGE 0xC110C8ED

// Size classes: 16 to 32KB
static const size_t class_sizes[NUM_CLASSES] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024,
    1280, 1536, 1792, 2048, 2560, 3072, 3584, 4096, 5120, 6144,
    7168, 8192, 10240, 12288, 14336, 16384, 20480, 24576, 28672, 32768
};

typedef struct block_header {
    uint32_t class_id;      
    uint32_t magic;         
    size_t size;            
} block_header;

// NOVELTY 2: Per-Class Locks (Upgraded from 1 Global Lock)
static pthread_mutex_t class_locks[NUM_CLASSES];
static pthread_once_t locks_init_once = PTHREAD_ONCE_INIT;

static void init_locks(void) {
    for (int i = 0; i < NUM_CLASSES; i++) {
        pthread_mutex_init(&class_locks[i], NULL);
    }
}

typedef struct free_block {
    struct free_block *next;
} free_block;

static free_block *free_lists[NUM_CLASSES] = {0};

static int get_class_id(size_t size) {
    if (size == 0) return 0; 
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (size <= class_sizes[i]) {
            return i;
        }
    }
    return -1;
}

static size_t round_up(size_t num, size_t multiple) {
    return (num + multiple - 1) / multiple * multiple;
}

void *my_malloc(size_t size) {
    if (size == 0) size = 1;

    // 1. Large Allocation Path
    if (size > LARGE_THRESHOLD) {
        if (size > SIZE_MAX - sizeof(block_header)) {
            errno = ENOMEM;
            return NULL;
        }
        size_t total_size = round_up(size + sizeof(block_header), 4096);
        void *mapping = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED) { errno = ENOMEM; return NULL; }
        
        block_header *header = (block_header *)mapping;
        header->class_id = LARGE_CLASS_ID;
        header->magic = MAGIC_LARGE; // Marked as large/fresh
        header->size = size;
        return (void *)(header + 1);
    }

    // 2. Small Allocation Path
    int class_id = get_class_id(size);
    if (class_id == -1) { errno = ENOMEM; return NULL; }

    pthread_once(&locks_init_once, init_locks);
    pthread_mutex_lock(&class_locks[class_id]);

    block_header *header = NULL;

    if (free_lists[class_id] != NULL) {
        free_block *fb = free_lists[class_id];
        free_lists[class_id] = fb->next;
        header = (block_header *)((char *)fb - sizeof(block_header));
    } else {
        void *arena = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (arena == MAP_FAILED) {
            pthread_mutex_unlock(&class_locks[class_id]);
            errno = ENOMEM; 
            return NULL; 
        }

        size_t block_total = class_sizes[class_id] + sizeof(block_header);
        size_t num_blocks = ARENA_SIZE / block_total;

        for (size_t i = 0; i < num_blocks; i++) {
            block_header *b = (block_header *)((char *)arena + (i * block_total));
            b->class_id = class_id;
            b->magic = MAGIC_LIVE;
            b->size = class_sizes[class_id];
            
            free_block *fb = (free_block *)((char *)b + sizeof(block_header));
            fb->next = free_lists[class_id];
            free_lists[class_id] = fb;
        }

        free_block *fb = free_lists[class_id];
        free_lists[class_id] = fb->next;
        header = (block_header *)((char *)fb - sizeof(block_header));
    }

    pthread_mutex_unlock(&class_locks[class_id]);
    header->size = size;
    header->magic = MAGIC_LIVE;
    return (void *)(header + 1);
}

void my_free(void *ptr) {
    if (ptr == NULL) return;

    block_header *header = ((block_header *)ptr) - 1;

    #ifdef MEMCTL_DEBUG
    // NOVELTY 2: Debug Mode Catch
    if (header->magic != MAGIC_LIVE && header->magic != MAGIC_LARGE) {
        const char *err = "[DEBUG] ERROR: Double free or invalid pointer detected!\n";
        write(2, err, 55);
        return;
    }
    #endif

    // 1. Large Free Path
    if (header->class_id == LARGE_CLASS_ID) {
        size_t total_size = round_up(header->size + sizeof(block_header), 4096);
        munmap(header, total_size);
        return;
    }

    // 2. Small Free Path
    pthread_once(&locks_init_once, init_locks);
    pthread_mutex_lock(&class_locks[header->class_id]);

    #ifdef MEMCTL_DEBUG
    header->magic = MAGIC_FREE;
    memset(ptr, 0xDD, header->size); // Poison
    #endif
    
    free_block *fb = (free_block *)ptr;
    fb->next = free_lists[header->class_id];
    free_lists[header->class_id] = fb;
    
    pthread_mutex_unlock(&class_locks[header->class_id]);
}

void *my_calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        errno = ENOMEM;
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = my_malloc(total);
    
    if (ptr != NULL && total > 0) {
        block_header *header = ((block_header *)ptr) - 1;
        // NOVELTY 3: Zero-Skip for Large blocks (fresh from mmap)
        if (header->class_id == LARGE_CLASS_ID) {
            header->magic = MAGIC_LIVE; // Mark as used so free() works
        } else {
            memset(ptr, 0, total); 
        }
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return my_malloc(size);
    if (size == 0) { my_free(ptr); return NULL; }

    if (size > SIZE_MAX - sizeof(block_header)) {
        errno = ENOMEM;
        return NULL;
    }

    block_header *header = ((block_header *)ptr) - 1;

    // NOVELTY 1: In-Place Realloc
    if (header->class_id != LARGE_CLASS_ID) {
        size_t current_capacity = class_sizes[header->class_id];
        if (size <= current_capacity && size > (current_capacity / 2)) {
            header->size = size; 
            return ptr; 
        }
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) return NULL; 

    size_t copy_size = (header->size < size) ? header->size : size;
    memcpy(new_ptr, ptr, copy_size);
    my_free(ptr);
    return new_ptr;
}
