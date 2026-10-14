#ifndef AZMA_COMMON_H
#define AZMA_COMMON_H

/*
 * Common.h
 * Shared basic types, utilities, assertions, and allocation helpers
 * for the AzmaIDL project.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =========================
   Platform / compiler
   ========================= */

#if defined(_MSC_VER)
#  define AZMA_COMPILER_MSVC 1
#else
#  define AZMA_COMPILER_MSVC 0
#endif

#if defined(__clang__)
#  define AZMA_COMPILER_CLANG 1
#else
#  define AZMA_COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  define AZMA_COMPILER_GCC 1
#else
#  define AZMA_COMPILER_GCC 0
#endif

#if defined(_WIN32) || defined(_WIN64)
#  define AZMA_PLATFORM_WINDOWS 1
#else
#  define AZMA_PLATFORM_WINDOWS 0
#endif

/* =========================
   Language linkage
   ========================= */

#ifdef __cplusplus
#  define AZMA_EXTERN_C_BEGIN extern "C" {
#  define AZMA_EXTERN_C_END   }
#else
#  define AZMA_EXTERN_C_BEGIN
#  define AZMA_EXTERN_C_END
#endif

AZMA_EXTERN_C_BEGIN

/* =========================
   Attributes / keywords
   ========================= */

#if AZMA_COMPILER_MSVC
#  define AZMA_INLINE __forceinline
#  define AZMA_NOINLINE __declspec(noinline)
#  define AZMA_UNUSED
#  define AZMA_NODISCARD _Check_return_
#elif AZMA_COMPILER_GCC || AZMA_COMPILER_CLANG
#  define AZMA_INLINE inline __attribute__((always_inline))
#  define AZMA_NOINLINE __attribute__((noinline))
#  define AZMA_UNUSED __attribute__((unused))
#  define AZMA_NODISCARD __attribute__((warn_unused_result))
#else
#  define AZMA_INLINE inline
#  define AZMA_NOINLINE
#  define AZMA_UNUSED
#  define AZMA_NODISCARD
#endif

#if AZMA_COMPILER_GCC || AZMA_COMPILER_CLANG
#  define AZMA_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define AZMA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define AZMA_LIKELY(x)   (x)
#  define AZMA_UNLIKELY(x) (x)
#endif

#if __STDC_VERSION__ >= 201112L
#  define AZMA_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
#  define AZMA_STATIC_ASSERT(cond, msg) typedef char azma_static_assertion_##__LINE__[(cond) ? 1 : -1]
#endif

#define AZMA_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define AZMA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AZMA_MAX(a, b) ((a) > (b) ? (a) : (b))
#define AZMA_CLAMP(x, lo, hi) (AZMA_MIN(AZMA_MAX((x), (lo)), (hi)))

/* =========================
   Fundamental result types
   ========================= */

typedef enum AzmaStatus {
    AZMA_STATUS_OK = 0,
    AZMA_STATUS_ERROR = 1,
    AZMA_STATUS_OOM = 2,
    AZMA_STATUS_INVALID_ARGUMENT = 3,
    AZMA_STATUS_PARSE_ERROR = 4,
    AZMA_STATUS_INTERNAL_ERROR = 5,
    AZMA_STATUS_NOT_IMPLEMENTED = 6
} AzmaStatus;

typedef struct AzmaSpan {
    const char *data;
    size_t size;
} AzmaSpan;

typedef struct AzmaString {
    char *data;
    size_t size;
    size_t capacity;
} AzmaString;

typedef struct AzmaSourceLoc {
    const char *file;
    size_t line;
    size_t column;
    size_t offset;
} AzmaSourceLoc;

typedef struct AzmaSourcePos {
    size_t line;
    size_t column;
    size_t offset;
} AzmaSourcePos;

typedef struct AzmaSourceRange {
    AzmaSourcePos begin;
    AzmaSourcePos end;
} AzmaSourceRange;

/* =========================
   Allocator
   ========================= */

typedef void *(*AzmaAllocFn)(void *user, size_t size);
typedef void *(*AzmaReallocFn)(void *user, void *ptr, size_t new_size);
typedef void  (*AzmaFreeFn)(void *user, void *ptr);

typedef struct AzmaAllocator {
    void *user;
    AzmaAllocFn alloc;
    AzmaReallocFn realloc;
    AzmaFreeFn free;
} AzmaAllocator;

static AZMA_INLINE void *azma_default_alloc(void *user, size_t size) {
    (void)user;
    return malloc(size);
}

static AZMA_INLINE void *azma_default_realloc(void *user, void *ptr, size_t new_size) {
    (void)user;
    return realloc(ptr, new_size);
}

static AZMA_INLINE void azma_default_free(void *user, void *ptr) {
    (void)user;
    free(ptr);
}

static AZMA_INLINE AzmaAllocator azma_allocator_default(void) {
    AzmaAllocator a;
    a.user = NULL;
    a.alloc = azma_default_alloc;
    a.realloc = azma_default_realloc;
    a.free = azma_default_free;
    return a;
}

static AZMA_INLINE void *azma_alloc(AzmaAllocator *a, size_t size) {
    if (!a || !a->alloc) return NULL;
    return a->alloc(a->user, size);
}

static AZMA_INLINE void *azma_realloc(AzmaAllocator *a, void *ptr, size_t size) {
    if (!a || !a->realloc) return NULL;
    return a->realloc(a->user, ptr, size);
}

static AZMA_INLINE void azma_free(AzmaAllocator *a, void *ptr) {
    if (!a || !a->free) return;
    a->free(a->user, ptr);
}

/* =========================
   Diagnostics
   ========================= */

typedef void (*AzmaLogFn)(void *user, const char *message);

typedef struct AzmaLogger {
    void *user;
    AzmaLogFn log;
} AzmaLogger;

static AZMA_INLINE void azma_logf(AzmaLogger *logger, const char *fmt, ...) {
    if (!logger || !logger->log || !fmt) return;

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
#if AZMA_COMPILER_MSVC
    _vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    buffer[sizeof(buffer) - 1] = '\0';
#else
    vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
    va_end(args);

    logger->log(logger->user, buffer);
}

/* =========================
   Assertions / panic
   ========================= */

typedef void (*AzmaPanicFn)(void *user, const char *expr, const char *file, int line);

typedef struct AzmaPanicHandler {
    void *user;
    AzmaPanicFn panic;
} AzmaPanicHandler;

static AZMA_INLINE void azma_default_panic(void *user, const char *expr, const char *file, int line) {
    (void)user;
    fprintf(stderr, "AZMA PANIC: assertion failed: (%s) at %s:%d\n", expr, file, line);
    abort();
}

static AZMA_INLINE AzmaPanicHandler azma_panic_handler_default(void) {
    AzmaPanicHandler h;
    h.user = NULL;
    h.panic = azma_default_panic;
    return h;
}

#ifndef AZMA_PANIC_HANDLER
#  define AZMA_PANIC_HANDLER azma_panic_handler_default()
#endif

#define AZMA_ASSERT(expr)                                                         \
    do {                                                                          \
        if (AZMA_UNLIKELY(!(expr))) {                                             \
            AzmaPanicHandler azma__panic_handler = AZMA_PANIC_HANDLER;            \
            azma__panic_handler.panic(                                            \
                azma__panic_handler.user, #expr, __FILE__, __LINE__);             \
        }                                                                         \
    } while (0)

/* =========================
   Memory helpers
   ========================= */

static AZMA_INLINE void *azma_memdup(AzmaAllocator *a, const void *src, size_t size) {
    if (!a) return NULL;
    if (size == 0) return NULL;
    void *dst = azma_alloc(a, size);
    if (!dst) return NULL;
    memcpy(dst, src, size);
    return dst;
}

static AZMA_INLINE char *azma_strdup(AzmaAllocator *a, const char *s) {
    if (!a || !s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)azma_alloc(a, n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static AZMA_INLINE char *azma_strndup(AzmaAllocator *a, const char *s, size_t n) {
    if (!a || !s) return NULL;
    char *out = (char *)azma_alloc(a, n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* =========================
   Span helpers
   ========================= */

static AZMA_INLINE AzmaSpan azma_span_from_parts(const char *data, size_t size) {
    AzmaSpan s;
    s.data = data;
    s.size = size;
    return s;
}

static AZMA_INLINE AzmaSpan azma_span_from_cstr(const char *s) {
    AzmaSpan out;
    out.data = s;
    out.size = s ? strlen(s) : 0;
    return out;
}

static AZMA_INLINE bool azma_span_eq(AzmaSpan a, AzmaSpan b) {
    if (a.size != b.size) return false;
    if (a.data == b.data) return true;
    if (!a.data || !b.data) return false;
    return memcmp(a.data, b.data, a.size) == 0;
}

/* =========================
   Dynamic string
   ========================= */

static AZMA_INLINE void azma_string_init(AzmaString *s) {
    if (!s) return;
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

static AZMA_INLINE void azma_string_destroy(AzmaString *s, AzmaAllocator *a) {
    if (!s || !a) return;
    if (s->data) {
        azma_free(a, s->data);
    }
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

static AZMA_INLINE bool azma_string_reserve(AzmaString *s, AzmaAllocator *a, size_t needed) {
    if (!s || !a) return false;
    if (needed <= s->capacity) return true;

    size_t new_cap = s->capacity ? s->capacity : 16;
    while (new_cap < needed) {
        if (new_cap > ((size_t)-1) / 2) {
            new_cap = needed;
            break;
        }
        new_cap *= 2;
    }

    char *new_data = (char *)azma_realloc(a, s->data, new_cap);
    if (!new_data) return false;

    s->data = new_data;
    s->capacity = new_cap;
    return true;
}

static AZMA_INLINE bool azma_string_append_n(
    AzmaString *s,
    AzmaAllocator *a,
    const char *data,
    size_t n
) {
    if (!s || !a || (!data && n != 0)) return false;
    if (!azma_string_reserve(s, a, s->size + n + 1)) return false;
    if (n) memcpy(s->data + s->size, data, n);
    s->size += n;
    s->data[s->size] = '\0';
    return true;
}

static AZMA_INLINE bool azma_string_append_cstr(
    AzmaString *s,
    AzmaAllocator *a,
    const char *text
) {
    if (!text) return false;
    return azma_string_append_n(s, a, text, strlen(text));
}

static AZMA_INLINE bool azma_string_push(
    AzmaString *s,
    AzmaAllocator *a,
    char ch
) {
    return azma_string_append_n(s, a, &ch, 1);
}

static AZMA_INLINE void azma_string_clear(AzmaString *s) {
    if (!s) return;
    s->size = 0;
    if (s->data) s->data[0] = '\0';
}

/* =========================
   Source location helpers
   ========================= */

static AZMA_INLINE AzmaSourceLoc azma_source_loc_make(
    const char *file,
    size_t line,
    size_t column,
    size_t offset
) {
    AzmaSourceLoc loc;
    loc.file = file;
    loc.line = line;
    loc.column = column;
    loc.offset = offset;
    return loc;
}

/* =========================
   Status helpers
   ========================= */

static AZMA_INLINE const char *azma_status_string(AzmaStatus status) {
    switch (status) {
        case AZMA_STATUS_OK: return "ok";
        case AZMA_STATUS_ERROR: return "error";
        case AZMA_STATUS_OOM: return "out_of_memory";
        case AZMA_STATUS_INVALID_ARGUMENT: return "invalid_argument";
        case AZMA_STATUS_PARSE_ERROR: return "parse_error";
        case AZMA_STATUS_INTERNAL_ERROR: return "internal_error";
        case AZMA_STATUS_NOT_IMPLEMENTED: return "not_implemented";
        default: return "unknown_status";
    }
}

/* =========================
   Versioning
   ========================= */

#define AZMA_VERSION_MAJOR 0
#define AZMA_VERSION_MINOR 1
#define AZMA_VERSION_PATCH 0

/* =========================
   Sanity checks
   ========================= */

AZMA_STATIC_ASSERT(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
AZMA_STATIC_ASSERT(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
AZMA_STATIC_ASSERT(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
AZMA_STATIC_ASSERT(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

AZMA_EXTERN_C_END

#endif /* AZMA_COMMON_H */
