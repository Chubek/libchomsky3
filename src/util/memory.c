#include "chomsky3/util/memory.h"
#include "chomsky3/util/error.h"
#include "chomsky3/util/debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Memory tracking structure */
typedef struct memory_block {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    const char *function;
    struct memory_block *next;
} memory_block_t;

/* Global memory tracking */
static memory_block_t *g_memory_blocks = NULL;
static size_t g_total_allocated = 0;
static size_t g_total_freed = 0;
static size_t g_current_allocated = 0;
static size_t g_peak_allocated = 0;
static size_t g_allocation_count = 0;
static size_t g_free_count = 0;
static int g_memory_tracking_enabled = 0;

/* Memory pool structure */
struct chomsky3_memory_pool {
    void *memory;
    size_t size;
    size_t used;
    size_t alignment;
    struct chomsky3_memory_pool *next;
};

/* Enable/disable memory tracking */
void chomsky3_memory_tracking_enable(int enable) {
    g_memory_tracking_enabled = enable;
}

/* Check if memory tracking is enabled */
int chomsky3_memory_tracking_is_enabled(void) {
    return g_memory_tracking_enabled;
}

/* Add memory block to tracking list */
static void track_allocation(void *ptr, size_t size, const char *file, int line, const char *function) {
    if (!g_memory_tracking_enabled || !ptr) {
        return;
    }

    memory_block_t *block = malloc(sizeof(memory_block_t));
    if (!block) {
        return;
    }

    block->ptr = ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->function = function;
    block->next = g_memory_blocks;
    g_memory_blocks = block;

    g_total_allocated += size;
    g_current_allocated += size;
    g_allocation_count++;

    if (g_current_allocated > g_peak_allocated) {
        g_peak_allocated = g_current_allocated;
    }

    CHOMSKY3_TRACE("Allocated %zu bytes at %p (%s:%d in %s)", 
                   size, ptr, file ? file : "unknown", line, function ? function : "unknown");
}

/* Remove memory block from tracking list */
static void track_deallocation(void *ptr, const char *file, int line, const char *function) {
    if (!g_memory_tracking_enabled || !ptr) {
        return;
    }

    memory_block_t *prev = NULL;
    memory_block_t *current = g_memory_blocks;

    while (current) {
        if (current->ptr == ptr) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_memory_blocks = current->next;
            }

            g_total_freed += current->size;
            g_current_allocated -= current->size;
            g_free_count++;

            CHOMSKY3_TRACE("Freed %zu bytes at %p (%s:%d in %s)", 
                           current->size, ptr, file ? file : "unknown", line, function ? function : "unknown");

            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }

    CHOMSKY3_WARN("Attempted to free untracked pointer %p (%s:%d in %s)", 
                  ptr, file ? file : "unknown", line, function ? function : "unknown");
}

/* Allocate memory (internal) */
void *chomsky3_malloc_internal(size_t size, const char *file, int line, const char *function) {
    void *ptr = malloc(size);
    if (!ptr) {
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, "Failed to allocate %zu bytes", size);
        return NULL;
    }

    track_allocation(ptr, size, file, line, function);
    return ptr;
}

/* Allocate zeroed memory (internal) */
void *chomsky3_calloc_internal(size_t nmemb, size_t size, const char *file, int line, const char *function) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, "Failed to allocate %zu bytes", nmemb * size);
        return NULL;
    }

    track_allocation(ptr, nmemb * size, file, line, function);
    return ptr;
}

/* Reallocate memory (internal) */
void *chomsky3_realloc_internal(void *ptr, size_t size, const char *file, int line, const char *function) {
    if (!ptr) {
        return chomsky3_malloc_internal(size, file, line, function);
    }

    if (size == 0) {
        chomsky3_free_internal(ptr, file, line, function);
        return NULL;
    }

    /* Track deallocation of old pointer */
    track_deallocation(ptr, file, line, function);

    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, "Failed to reallocate %zu bytes", size);
        /* Re-track old pointer since realloc failed */
        track_allocation(ptr, 0, file, line, function);
        return NULL;
    }

    track_allocation(new_ptr, size, file, line, function);
    return new_ptr;
}

/* Free memory (internal) */
void chomsky3_free_internal(void *ptr, const char *file, int line, const char *function) {
    if (!ptr) {
        return;
    }

    track_deallocation(ptr, file, line, function);
    free(ptr);
}

/* Duplicate string (internal) */
char *chomsky3_strdup_internal(const char *str, const char *file, int line, const char *function) {
    if (!str) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
    char *dup = chomsky3_malloc_internal(len, file, line, function);
    if (!dup) {
        return NULL;
    }

    memcpy(dup, str, len);
    return dup;
}

/* Duplicate string with length limit (internal) */
char *chomsky3_strndup_internal(const char *str, size_t n, const char *file, int line, const char *function) {
    if (!str) {
        return NULL;
    }

    size_t len = strnlen(str, n);
    char *dup = chomsky3_malloc_internal(len + 1, file, line, function);
    if (!dup) {
        return NULL;
    }

    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

/* Get memory statistics */
void chomsky3_memory_get_stats(chomsky3_memory_stats_t *stats) {
    if (!stats) {
        return;
    }

    stats->total_allocated = g_total_allocated;
    stats->total_freed = g_total_freed;
    stats->current_allocated = g_current_allocated;
    stats->peak_allocated = g_peak_allocated;
    stats->allocation_count = g_allocation_count;
    stats->free_count = g_free_count;
}

/* Print memory statistics */
void chomsky3_memory_print_stats(void) {
    chomsky3_memory_stats_t stats;
    chomsky3_memory_get_stats(&stats);

    printf("Memory Statistics:\n");
    printf("  Total allocated:   %zu bytes (%zu allocations)\n", 
           stats.total_allocated, stats.allocation_count);
    printf("  Total freed:       %zu bytes (%zu frees)\n", 
           stats.total_freed, stats.free_count);
    printf("  Current allocated: %zu bytes\n", stats.current_allocated);
    printf("  Peak allocated:    %zu bytes\n", stats.peak_allocated);
    printf("  Leaked:            %zu bytes (%zu blocks)\n", 
           stats.current_allocated, stats.allocation_count - stats.free_count);
}

/* Check for memory leaks */
int chomsky3_memory_check_leaks(void) {
    if (!g_memory_tracking_enabled) {
        CHOMSKY3_WARN("Memory tracking is not enabled");
        return 0;
    }

    int leak_count = 0;
    memory_block_t *current = g_memory_blocks;

    if (!current) {
        CHOMSKY3_INFO("No memory leaks detected");
        return 0;
    }

    CHOMSKY3_ERROR("Memory leaks detected:");

    while (current) {
        CHOMSKY3_ERROR("  Leak #%d: %zu bytes at %p (allocated at %s:%d in %s)",
                       leak_count + 1,
                       current->size,
                       current->ptr,
                       current->file ? current->file : "unknown",
                       current->line,
                       current->function ? current->function : "unknown");
        leak_count++;
        current = current->next;
    }

    return leak_count;
}

/* Free all tracked memory (for cleanup) */
void chomsky3_memory_cleanup(void) {
    memory_block_t *current = g_memory_blocks;

    while (current) {
        memory_block_t *next = current->next;
        free(current->ptr);
        free(current);
        current = next;
    }

    g_memory_blocks = NULL;
    g_current_allocated = 0;
}

/* Reset memory statistics */
void chomsky3_memory_reset_stats(void) {
    g_total_allocated = 0;
    g_total_freed = 0;
    g_current_allocated = 0;
    g_peak_allocated = 0;
    g_allocation_count = 0;
    g_free_count = 0;
}

/* Memory pool functions */

/* Create memory pool */
chomsky3_memory_pool_t *chomsky3_memory_pool_create(size_t size, size_t alignment) {
    if (size == 0) {
        chomsky3_set_error(CHOMSKY3_ERROR_INVALID_ARGUMENT, "Pool size cannot be zero");
        return NULL;
    }

    if (alignment == 0) {
        alignment = sizeof(void *);
    }

    chomsky3_memory_pool_t *pool = malloc(sizeof(chomsky3_memory_pool_t));
    if (!pool) {
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, "Failed to allocate memory pool");
        return NULL;
    }

    pool->memory = malloc(size);
    if (!pool->memory) {
        free(pool);
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, "Failed to allocate pool memory");
        return NULL;
    }

    pool->size = size;
    pool->used = 0;
    pool->alignment = alignment;
    pool->next = NULL;

    CHOMSKY3_DEBUG("Created memory pool: %zu bytes with %zu-byte alignment", size, alignment);

    return pool;
}

/* Allocate from memory pool */
void *chomsky3_memory_pool_alloc(chomsky3_memory_pool_t *pool, size_t size) {
    if (!pool || size == 0) {
        return NULL;
    }

    /* Align size */
    size_t aligned_size = (size + pool->alignment - 1) & ~(pool->alignment - 1);

    /* Check if we have enough space */
    if (pool->used + aligned_size > pool->size) {
        chomsky3_set_error(CHOMSKY3_ERROR_OUT_OF_MEMORY, 
                          "Pool exhausted: requested %zu bytes, available %zu bytes",
                          aligned_size, pool->size - pool->used);
        return NULL;
    }

    void *ptr = (char *)pool->memory + pool->used;
    pool->used += aligned_size;

    CHOMSKY3_TRACE("Pool allocated %zu bytes at %p (%zu/%zu used)", 
                   size, ptr, pool->used, pool->size);

    return ptr;
}

/* Reset memory pool */
void chomsky3_memory_pool_reset(chomsky3_memory_pool_t *pool) {
    if (!pool) {
        return;
    }

    CHOMSKY3_DEBUG("Resetting memory pool (%zu bytes freed)", pool->used);
    pool->used = 0;
}

/* Destroy memory pool */
void chomsky3_memory_pool_destroy(chomsky3_memory_pool_t *pool) {
    if (!pool) {
        return;
    }

    CHOMSKY3_DEBUG("Destroying memory pool (%zu bytes)", pool->size);

    free(pool->memory);
    free(pool);
}

/* Get pool statistics */
void chomsky3_memory_pool_get_stats(chomsky3_memory_pool_t *pool, 
                                     size_t *size, 
                                     size_t *used, 
                                     size_t *available) {
    if (!pool) {
        return;
    }

    if (size) {
        *size = pool->size;
    }
    if (used) {
        *used = pool->used;
    }
    if (available) {
        *available = pool->size - pool->used;
    }
}

/* Safe memory operations */

/* Safe memcpy with bounds checking */
int chomsky3_memcpy_safe(void *dest, size_t dest_size, const void *src, size_t count) {
    if (!dest || !src) {
        chomsky3_set_error(CHOMSKY3_ERROR_INVALID_ARGUMENT, "NULL pointer in memcpy");
        return -1;
    }

    if (count > dest_size) {
        chomsky3_set_error(CHOMSKY3_ERROR_BUFFER_OVERFLOW, 
                          "Buffer overflow: copying %zu bytes to %zu-byte buffer",
                          count, dest_size);
        return -1;
    }

    memcpy(dest, src, count);
    return 0;
}

/* Safe memmove with bounds checking */
int chomsky3_memmove_safe(void *dest, size_t dest_size, const void *src, size_t count) {
    if (!dest || !src) {
        chomsky3_set_error(CHOMSKY3_ERROR_INVALID_ARGUMENT, "NULL pointer in memmove");
        return -1;
    }

    if (count > dest_size) {
        chomsky3_set_error(CHOMSKY3_ERROR_BUFFER_OVERFLOW, 
                          "Buffer overflow: moving %zu bytes to %zu-byte buffer",
                          count, dest_size);
        return -1;
    }

    memmove(dest, src, count);
    return 0;
}

/* Safe memset with bounds checking */
int chomsky3_memset_safe(void *dest, size_t dest_size, int value, size_t count) {
    if (!dest) {
        chomsky3_set_error(CHOMSKY3_ERROR_INVALID_ARGUMENT, "NULL pointer in memset");
        return -1;
    }

    if (count > dest_size) {
        chomsky3_set_error(CHOMSKY3_ERROR_BUFFER_OVERFLOW, 
                          "Buffer overflow: setting %zu bytes in %zu-byte buffer",
                          count, dest_size);
        return -1;
    }

    memset(dest, value, count);
    return 0;
}

/* Secure memory zeroing (prevents optimization) */
void chomsky3_memzero_secure(void *ptr, size_t size) {
    if (!ptr || size == 0) {
        return;
    }

    volatile unsigned char *p = ptr;
    while (size--) {
        *p++ = 0;
    }
}
