/**
 * Internal memory management compatibility layer.
 */

#ifndef CHOMSKY3_UTIL_MEMORY_H
#define CHOMSKY3_UTIL_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
} chomsky3_memory_stats_t;

typedef struct chomsky3_memory_pool chomsky3_memory_pool_t;

void *chomsky3_malloc_internal(size_t size, const char *file, int line, const char *function);
void *chomsky3_calloc_internal(size_t nmemb, size_t size, const char *file, int line, const char *function);
void *chomsky3_realloc_internal(void *ptr, size_t size, const char *file, int line, const char *function);
void chomsky3_free_internal(void *ptr, const char *file, int line, const char *function);
char *chomsky3_strdup_internal(const char *str, const char *file, int line, const char *function);
char *chomsky3_strndup_internal(const char *str, size_t n, const char *file, int line, const char *function);

#define chomsky3_malloc(size) \
    chomsky3_malloc_internal((size), __FILE__, __LINE__, __func__)
#define chomsky3_calloc(nmemb, size) \
    chomsky3_calloc_internal((nmemb), (size), __FILE__, __LINE__, __func__)
#define chomsky3_realloc(ptr, size) \
    chomsky3_realloc_internal((ptr), (size), __FILE__, __LINE__, __func__)
#define chomsky3_free(ptr) \
    chomsky3_free_internal((ptr), __FILE__, __LINE__, __func__)
#define chomsky3_strdup(str) \
    chomsky3_strdup_internal((str), __FILE__, __LINE__, __func__)
#define chomsky3_strndup(str, n) \
    chomsky3_strndup_internal((str), (n), __FILE__, __LINE__, __func__)

void chomsky3_memory_tracking_enable(int enable);
int chomsky3_memory_tracking_is_enabled(void);
void chomsky3_memory_get_stats(chomsky3_memory_stats_t *stats);
void chomsky3_memory_print_stats(void);
int chomsky3_memory_check_leaks(void);
void chomsky3_memory_cleanup(void);
void chomsky3_memory_reset_stats(void);
chomsky3_memory_pool_t *chomsky3_memory_pool_create(size_t size, size_t alignment);
void *chomsky3_memory_pool_alloc(chomsky3_memory_pool_t *pool, size_t size);
void chomsky3_memory_pool_reset(chomsky3_memory_pool_t *pool);
void chomsky3_memory_pool_destroy(chomsky3_memory_pool_t *pool);
void chomsky3_memory_pool_get_stats(chomsky3_memory_pool_t *pool,
                                   size_t *size,
                                   size_t *used,
                                   size_t *available);
int chomsky3_memcpy_safe(void *dest, size_t dest_size, const void *src, size_t count);
int chomsky3_memmove_safe(void *dest, size_t dest_size, const void *src, size_t count);
int chomsky3_memset_safe(void *dest, size_t dest_size, int value, size_t count);
void chomsky3_memzero_secure(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CHOMSKY3_UTIL_MEMORY_H */
