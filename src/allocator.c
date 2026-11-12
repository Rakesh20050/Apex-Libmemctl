#include "libmemctl.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define ARENA_SIZE (1024 * 1024)    // 1 MB Arenas
#define LARGE_THRESHOLD (32 * 1024) // Anything > 32KB is "Large"
#define NUM_CLASSES 40
#define LARGE_CLASS_ID 999
#define CACHE_MAX_BLOCKS 32         // Max blocks per size class per thread

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

typedef struct free_block {
    struct free_block *next;
} free_block;

// Global Free Lists (Shared across all threads)
static free_block *free_lists[NUM_CLASSES] = {0};

// Per-Class Locks
static pthread_mutex_t class_locks[NUM_CLASSES];
static pthread_once_t locks_init_once = PTHREAD_ONCE_INIT;

static void init_locks(void) {
    for (int i = 0; i < NUM_CLASSES; i++) {
        pthread_mutex_init(&class_locks[i], NULL);
    }
}

// NOVELTY 4: Thread Caches (Zero-Lock Fast Path)
typedef struct {
    free_block *head;
    int count;
} thread_cache_t;

static _Thread_local thread_cache_t t_cache[NUM_CLASSES];

// Thread Exit Destructor Setup
static pthread_key_t cache_key;
static pthread_once_t cache_key_once = PTHREAD_ONCE_INIT;

static void flush_thread_cache(void *arg);
static void create_cache_key(void);

static void create_cache_key(void) {
    pthread_key_create(&cache_key, flush_thread_cache);
}

// This runs automatically when a thread exits to prevent leaks
static void flush_thread_cache(void *arg) {
    (void)arg;
    pthread_once(&locks_init_once, init_locks);
    
    for (int i = 0; i < NUM_CLASSES; i++) {
        while (t_cache[i].head != NULL) {
            free_block *fb = t_cache[i].head;
            t_cache[i].head = fb->next;
            
            pthread_mutex_lock(&class_locks[i]);
            fb->next = free_lists[i];
            free_lists[i] = fb;
            pthread_mutex_unlock(&class_locks[i]);
        }
        t_cache[i].count = 0;
    }
}

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
    if (multiple == 0) return 0;
    size_t remainder = num % multiple;
    if (remainder == 0) return num;
    size_t padding = multiple - remainder;
    if (num > SIZE_MAX - padding) return 0;
    return num + padding;
}

void *my_malloc(size_t size) {
    if (size == 0) size = 1;

    // 1. Large Allocation Path (Direct mmap)
    if (size > LARGE_THRESHOLD) {
        if (size > SIZE_MAX - sizeof(block_header)) { errno = ENOMEM; return NULL; }
        size_t total_size = round_up(size + sizeof(block_header), 4096);
        if (total_size == 0) { errno = ENOMEM; return NULL; }
        
        void *mapping = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED) { errno = ENOMEM; return NULL; }
        
        block_header *header = (block_header *)mapping;
        header->class_id = LARGE_CLASS_ID;
        header->magic = MAGIC_LARGE;
        header->size = size;
        return (void *)(header + 1);
    }

    // 2. Small Allocation Path
    int class_id = get_class_id(size);
    if (class_id == -1) { errno = ENOMEM; return NULL; }

    // Register thread destructor
    pthread_once(&cache_key_once, create_cache_key);
    pthread_setspecific(cache_key, (void *)1); 

    block_header *header = NULL;

    // A. Check Thread Cache (Zero Locks!)
    if (t_cache[class_id].head != NULL) {
        free_block *fb = t_cache[class_id].head;
        t_cache[class_id].head = fb->next;
        t_cache[class_id].count--;
        header = (block_header *)((char *)fb - sizeof(block_header));
        header->size = size;
        header->magic = MAGIC_LIVE;
        return (void *)(header + 1);
    }

    // B. Cache Empty: Lock and refill from Global Free List
    pthread_once(&locks_init_once, init_locks);
    pthread_mutex_lock(&class_locks[class_id]);

    int fetched = 0;
    while (free_lists[class_id] != NULL && fetched < CACHE_MAX_BLOCKS) {
        free_block *fb = free_lists[class_id];
        free_lists[class_id] = fb->next;
        fb->next = t_cache[class_id].head;
        t_cache[class_id].head = fb;
        t_cache[class_id].count++;
        fetched++;
    }

    // C. Global Free List Empty: Carve new Arena
    if (t_cache[class_id].head == NULL) {
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
    b->magic = MAGIC_FREE;
    b->size = class_sizes[class_id];

    free_block *fb = (free_block *)((char *)b + sizeof(block_header));

    if (t_cache[class_id].count < CACHE_MAX_BLOCKS) {
        fb->next = t_cache[class_id].head;
        t_cache[class_id].head = fb;
        t_cache[class_id].count++;
    } else {
        fb->next = free_lists[class_id];
        free_lists[class_id] = fb;
    }
} 
   }

    pthread_mutex_unlock(&class_locks[class_id]);

    // Pop the first block for this allocation
    free_block *fb = t_cache[class_id].head;
    t_cache[class_id].head = fb->next;
    t_cache[class_id].count--;
    header = (block_header *)((char *)fb - sizeof(block_header));
    header->size = size;
    header->magic = MAGIC_LIVE;
    return (void *)(header + 1);
}

void my_free(void *ptr) {
    if (ptr == NULL) return;

    block_header *header = ((block_header *)ptr) - 1;

    #ifdef MEMCTL_DEBUG
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

    int class_id = header->class_id;

    // Register thread destructor (in case free is called before malloc)
    pthread_once(&cache_key_once, create_cache_key);
    pthread_setspecific(cache_key, (void *)1); 

    #ifdef MEMCTL_DEBUG
    header->magic = MAGIC_FREE;
    memset(ptr, 0xDD, header->size);
    #endif

    // 2. Small Free Path (Push to Thread Cache)
    if (t_cache[class_id].count < CACHE_MAX_BLOCKS) {
        free_block *fb = (free_block *)ptr;
        fb->next = t_cache[class_id].head;
        t_cache[class_id].head = fb;
        t_cache[class_id].count++;
    } else {
        // Cache full, flush this one to global list
        pthread_once(&locks_init_once, init_locks);
        pthread_mutex_lock(&class_locks[class_id]);
        free_block *fb = (free_block *)ptr;
        fb->next = free_lists[class_id];
        free_lists[class_id] = fb;
        pthread_mutex_unlock(&class_locks[class_id]);
    }
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
        // NOVELTY 3: Zero-Skip for Large blocks
        if (header->class_id == LARGE_CLASS_ID) {
            header->magic = MAGIC_LIVE;
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

#ifdef MEMCTL_DEBUG
#include <stdio.h>

// NOVELTY 5: Public Heap Visualizer (For Demo)
void debug_print_heap_map(void) {
    printf("\n=== HEAP MAP (Global Free Lists) ===\n");
    for (int i = 0; i < NUM_CLASSES; i++) {
        int count = 0;
        free_block *curr = free_lists[i];
        while (curr != NULL) {
            count++;
            curr = curr->next;
        }
        // Only print classes that have active free blocks
        if (count > 0) {
            printf("Class %2d (%4zu bytes) : ", i, class_sizes[i]);
            for (int j = 0; j < count && j < 20; j++) printf("[ ] "); 
            if (count > 20) printf("... (+%d more)", count - 20);
            printf("\n");
        }
    }
    printf("====================================\n\n");
}
#endif
