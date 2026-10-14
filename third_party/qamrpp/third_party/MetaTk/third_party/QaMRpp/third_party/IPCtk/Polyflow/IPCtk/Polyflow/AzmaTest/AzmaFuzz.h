#ifndef AZMA_FUZZ_H
#define AZMA_FUZZ_H

/*
 * AzmaFuzz.h
 * Tiny fuzzing harness helpers for AzmaIDL.
 *
 * Goals:
 *   - simple API
 *   - deterministic PRNG
 *   - byte and text mutation helpers
 *   - corpus entry abstraction
 *   - easy integration with parser entrypoints
 *
 * This is not a full engine. It is a compact utility layer for:
 *   - mutation-based parser fuzzing
 *   - regression corpus replay
 *   - smoke/property tests
 */

#include "Common.h"

AZMA_EXTERN_C_BEGIN

/* =========================
   Types
   ========================= */

typedef struct AzmaFuzzRng {
    uint64_t state;
} AzmaFuzzRng;

typedef struct AzmaFuzzBuffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
    AzmaAllocator allocator;
} AzmaFuzzBuffer;

typedef struct AzmaFuzzInput {
    const uint8_t *data;
    size_t size;
} AzmaFuzzInput;

typedef AzmaStatus (*AzmaFuzzTargetFn)(
    void *user,
    const uint8_t *data,
    size_t size
);

typedef struct AzmaFuzzOptions {
    uint64_t seed;
    size_t iterations;
    size_t max_input_size;
    int stop_on_failure;
    int print_cases;
} AzmaFuzzOptions;

typedef struct AzmaFuzzStats {
    size_t runs;
    size_t failures;
    size_t generated;
    size_t mutated;
    size_t bytes_tested;
} AzmaFuzzStats;

/* =========================
   RNG
   ========================= */

static AZMA_INLINE void azma_fuzz_rng_init(AzmaFuzzRng *rng, uint64_t seed) {
    AZMA_ASSERT(rng != NULL);
    if (seed == 0) {
        seed = 0x9E3779B97F4A7C15ull;
    }
    rng->state = seed;
}

static AZMA_INLINE uint64_t azma_fuzz_rng_next_u64(AzmaFuzzRng *rng) {
    uint64_t x;
    AZMA_ASSERT(rng != NULL);
    x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static AZMA_INLINE uint32_t azma_fuzz_rng_next_u32(AzmaFuzzRng *rng) {
    return (uint32_t)(azma_fuzz_rng_next_u64(rng) >> 32);
}

static AZMA_INLINE size_t azma_fuzz_rng_range(AzmaFuzzRng *rng, size_t upper_bound) {
    if (upper_bound == 0) return 0;
    return (size_t)(azma_fuzz_rng_next_u64(rng) % (uint64_t)upper_bound);
}

static AZMA_INLINE int azma_fuzz_rng_bool(AzmaFuzzRng *rng) {
    return (int)(azma_fuzz_rng_next_u64(rng) & 1ull);
}

static AZMA_INLINE uint8_t azma_fuzz_rng_byte(AzmaFuzzRng *rng) {
    return (uint8_t)(azma_fuzz_rng_next_u64(rng) & 0xFFu);
}

/* =========================
   Buffer
   ========================= */

static AZMA_INLINE void azma_fuzz_buffer_init(AzmaFuzzBuffer *buf, AzmaAllocator allocator) {
    AZMA_ASSERT(buf != NULL);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
    buf->allocator = allocator;
}

static AZMA_INLINE void azma_fuzz_buffer_destroy(AzmaFuzzBuffer *buf) {
    if (!buf) return;
    if (buf->data) {
        azma_free(&buf->allocator, buf->data);
    }
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

static AZMA_INLINE int azma_fuzz_buffer_reserve(AzmaFuzzBuffer *buf, size_t needed) {
    uint8_t *new_data;
    size_t new_capacity;

    AZMA_ASSERT(buf != NULL);

    if (needed <= buf->capacity) {
        return 1;
    }

    new_capacity = buf->capacity ? buf->capacity : 64;
    while (new_capacity < needed) {
        if (new_capacity > ((size_t)-1) / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }

    new_data = (uint8_t *)azma_realloc(&buf->allocator, buf->data, new_capacity);
    if (!new_data) {
        return 0;
    }

    buf->data = new_data;
    buf->capacity = new_capacity;
    return 1;
}

static AZMA_INLINE int azma_fuzz_buffer_resize(AzmaFuzzBuffer *buf, size_t new_size) {
    AZMA_ASSERT(buf != NULL);
    if (!azma_fuzz_buffer_reserve(buf, new_size)) {
        return 0;
    }
    buf->size = new_size;
    return 1;
}

static AZMA_INLINE int azma_fuzz_buffer_assign(
    AzmaFuzzBuffer *buf,
    const uint8_t *data,
    size_t size
) {
    AZMA_ASSERT(buf != NULL);

    if (!azma_fuzz_buffer_resize(buf, size)) {
        return 0;
    }

    if (size && data) {
        memcpy(buf->data, data, size);
    }
    return 1;
}

static AZMA_INLINE int azma_fuzz_buffer_append(
    AzmaFuzzBuffer *buf,
    const uint8_t *data,
    size_t size
) {
    size_t old_size;
    size_t new_size;

    AZMA_ASSERT(buf != NULL);

    old_size = buf->size;
    if (size > ((size_t)-1) - old_size) {
        return 0;
    }
    new_size = old_size + size;
    if (!azma_fuzz_buffer_resize(buf, new_size)) {
        return 0;
    }

    if (size && data) {
        memcpy(buf->data + old_size, data, size);
    }
    return 1;
}

/* =========================
   Helpers
   ========================= */

static AZMA_INLINE uint8_t azma_fuzz_pick_printable(AzmaFuzzRng *rng) {
    static const char table[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "_-+/*%&|^~=<>!?:;.,(){}[]@#\"' \t\r\n";
    return (uint8_t)table[azma_fuzz_rng_range(rng, sizeof(table) - 1)];
}

static AZMA_INLINE int azma_fuzz_generate_bytes(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *out,
    size_t size
) {
    size_t i;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(out != NULL);

    if (!azma_fuzz_buffer_resize(out, size)) {
        return 0;
    }

    for (i = 0; i < size; ++i) {
        out->data[i] = azma_fuzz_rng_byte(rng);
    }
    return 1;
}

static AZMA_INLINE int azma_fuzz_generate_text(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *out,
    size_t size
) {
    size_t i;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(out != NULL);

    if (!azma_fuzz_buffer_resize(out, size)) {
        return 0;
    }

    for (i = 0; i < size; ++i) {
        out->data[i] = azma_fuzz_pick_printable(rng);
    }
    return 1;
}

/* =========================
   Mutation operators
   ========================= */

static AZMA_INLINE int azma_fuzz_mutate_flip_bit(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf
) {
    size_t index;
    uint8_t bit;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size == 0) return 1;

    index = azma_fuzz_rng_range(rng, buf->size);
    bit = (uint8_t)(1u << azma_fuzz_rng_range(rng, 8));
    buf->data[index] ^= bit;
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate_set_byte(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf
) {
    size_t index;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size == 0) return 1;

    index = azma_fuzz_rng_range(rng, buf->size);
    buf->data[index] = azma_fuzz_rng_byte(rng);
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate_insert_byte(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf,
    size_t max_size
) {
    size_t index;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size >= max_size) return 1;

    if (buf->size == (size_t)-1) return 0;
    index = azma_fuzz_rng_range(rng, buf->size + 1);
    if (!azma_fuzz_buffer_resize(buf, buf->size + 1)) {
        return 0;
    }

    if (index < buf->size - 1) {
        memmove(buf->data + index + 1, buf->data + index, buf->size - index - 1);
    }

    buf->data[index] = azma_fuzz_rng_byte(rng);
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate_delete_byte(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf
) {
    size_t index;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size == 0) return 1;

    index = azma_fuzz_rng_range(rng, buf->size);

    if (index + 1 < buf->size) {
        memmove(buf->data + index, buf->data + index + 1, buf->size - index - 1);
    }

    buf->size -= 1;
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate_duplicate_range(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf,
    size_t max_size
) {
    size_t start;
    size_t len;
    size_t insert_at;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size == 0 || buf->size >= max_size) return 1;

    start = azma_fuzz_rng_range(rng, buf->size);
    len = azma_fuzz_rng_range(rng, buf->size - start) + 1;
    if (len > max_size - buf->size) {
        len = max_size - buf->size;
    }
    if (len == 0) return 1;

    insert_at = azma_fuzz_rng_range(rng, buf->size + 1);

    if (len > ((size_t)-1) - buf->size) {
        return 0;
    }
    if (!azma_fuzz_buffer_resize(buf, buf->size + len)) {
        return 0;
    }

    memmove(buf->data + insert_at + len, buf->data + insert_at, buf->size - insert_at - len);
    memcpy(buf->data + insert_at, buf->data + start, len);
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate_overwrite_with_text(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf
) {
    size_t index;
    size_t len;
    size_t i;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    if (buf->size == 0) return 1;

    index = azma_fuzz_rng_range(rng, buf->size);
    len = azma_fuzz_rng_range(rng, AZMA_MIN((size_t)16, buf->size - index)) + 1;

    for (i = 0; i < len; ++i) {
        buf->data[index + i] = azma_fuzz_pick_printable(rng);
    }
    return 1;
}

static AZMA_INLINE int azma_fuzz_mutate(
    AzmaFuzzRng *rng,
    AzmaFuzzBuffer *buf,
    size_t max_size
) {
    size_t op_count;
    size_t i;

    AZMA_ASSERT(rng != NULL);
    AZMA_ASSERT(buf != NULL);

    op_count = 1 + azma_fuzz_rng_range(rng, 8);

    for (i = 0; i < op_count; ++i) {
        switch (azma_fuzz_rng_range(rng, 6)) {
            case 0:
                if (!azma_fuzz_mutate_flip_bit(rng, buf)) return 0;
                break;
            case 1:
                if (!azma_fuzz_mutate_set_byte(rng, buf)) return 0;
                break;
            case 2:
                if (!azma_fuzz_mutate_insert_byte(rng, buf, max_size)) return 0;
                break;
            case 3:
                if (!azma_fuzz_mutate_delete_byte(rng, buf)) return 0;
                break;
            case 4:
                if (!azma_fuzz_mutate_duplicate_range(rng, buf, max_size)) return 0;
                break;
            case 5:
            default:
                if (!azma_fuzz_mutate_overwrite_with_text(rng, buf)) return 0;
                break;
        }
    }

    return 1;
}

/* =========================
   Corpus helpers
   ========================= */

static AZMA_INLINE AzmaFuzzInput azma_fuzz_input_from_bytes(
    const void *data,
    size_t size
) {
    AzmaFuzzInput in;
    in.data = (const uint8_t *)data;
    in.size = size;
    return in;
}

static AZMA_INLINE AzmaFuzzInput azma_fuzz_input_from_cstr(const char *text) {
    AzmaFuzzInput in;
    in.data = (const uint8_t *)text;
    in.size = text ? strlen(text) : 0;
    return in;
}

/* =========================
   Runner
   ========================= */

static AZMA_INLINE AzmaFuzzOptions azma_fuzz_options_default(void) {
    AzmaFuzzOptions opt;
    opt.seed = 0xC0FFEE123456789ull;
    opt.iterations = 1000;
    opt.max_input_size = 4096;
    opt.stop_on_failure = 1;
    opt.print_cases = 0;
    return opt;
}

static AZMA_INLINE void azma_fuzz_stats_init(AzmaFuzzStats *stats) {
    AZMA_ASSERT(stats != NULL);
    memset(stats, 0, sizeof(*stats));
}

static AZMA_INLINE AzmaStatus azma_fuzz_run_one(
    AzmaFuzzTargetFn target,
    void *user,
    const uint8_t *data,
    size_t size
) {
    if (!target) {
        return AZMA_STATUS_INVALID_ARGUMENT;
    }
    return target(user, data, size);
}

static AZMA_INLINE int azma_fuzz_print_input(FILE *f, const uint8_t *data, size_t size) {
    size_t i;

    if (!f) return 0;

    fprintf(f, "size=%zu data=", size);
    for (i = 0; i < size; ++i) {
        fprintf(f, "%02X", (unsigned)data[i]);
        if (i + 1 != size) fputc(' ', f);
    }
    fputc('\n', f);
    return 1;
}

static AZMA_INLINE AzmaStatus azma_fuzz_run(
    AzmaFuzzTargetFn target,
    void *user,
    const AzmaFuzzInput *corpus,
    size_t corpus_count,
    const AzmaFuzzOptions *options,
    AzmaFuzzStats *stats
) {
    AzmaFuzzOptions opt;
    AzmaFuzzRng rng;
    AzmaFuzzBuffer work;
    size_t i;

    if (!target || !options || !stats) {
        return AZMA_STATUS_INVALID_ARGUMENT;
    }
    if (corpus_count > 0 && !corpus) {
        return AZMA_STATUS_INVALID_ARGUMENT;
    }

    opt = *options;
    azma_fuzz_stats_init(stats);
    azma_fuzz_rng_init(&rng, opt.seed);
    azma_fuzz_buffer_init(&work, azma_allocator_default());

    for (i = 0; i < corpus_count; ++i) {
        AzmaStatus st;
        stats->runs++;
        stats->bytes_tested += corpus[i].size;

        if (opt.print_cases) {
            fprintf(stdout, "[fuzz][corpus %zu] ", i);
            azma_fuzz_print_input(stdout, corpus[i].data, corpus[i].size);
        }

        st = azma_fuzz_run_one(target, user, corpus[i].data, corpus[i].size);
        if (st != AZMA_STATUS_OK) {
            stats->failures++;
            if (opt.stop_on_failure) {
                azma_fuzz_buffer_destroy(&work);
                return st;
            }
        }
    }

    for (i = 0; i < opt.iterations; ++i) {
        size_t base_index;
        size_t gen_size;
        AzmaStatus st;

        if (corpus_count > 0 && azma_fuzz_rng_bool(&rng)) {
            base_index = azma_fuzz_rng_range(&rng, corpus_count);
            if (!azma_fuzz_buffer_assign(&work, corpus[base_index].data, corpus[base_index].size)) {
                azma_fuzz_buffer_destroy(&work);
                return AZMA_STATUS_OOM;
            }
            if (!azma_fuzz_mutate(&rng, &work, opt.max_input_size)) {
                azma_fuzz_buffer_destroy(&work);
                return AZMA_STATUS_OOM;
            }
            stats->mutated++;
        } else {
            gen_size = azma_fuzz_rng_range(&rng, opt.max_input_size + 1);
            if (azma_fuzz_rng_bool(&rng)) {
                if (!azma_fuzz_generate_text(&rng, &work, gen_size)) {
                    azma_fuzz_buffer_destroy(&work);
                    return AZMA_STATUS_OOM;
                }
            } else {
                if (!azma_fuzz_generate_bytes(&rng, &work, gen_size)) {
                    azma_fuzz_buffer_destroy(&work);
                    return AZMA_STATUS_OOM;
                }
            }
            stats->generated++;
        }

        stats->runs++;
        stats->bytes_tested += work.size;

        if (opt.print_cases) {
            fprintf(stdout, "[fuzz][iter %zu] ", i);
            azma_fuzz_print_input(stdout, work.data, work.size);
        }

        st = azma_fuzz_run_one(target, user, work.data, work.size);
        if (st != AZMA_STATUS_OK) {
            stats->failures++;
            if (opt.stop_on_failure) {
                azma_fuzz_buffer_destroy(&work);
                return st;
            }
        }
    }

    azma_fuzz_buffer_destroy(&work);
    return AZMA_STATUS_OK;
}

static AZMA_INLINE void azma_fuzz_print_stats(FILE *f, const AzmaFuzzStats *stats) {
    if (!f || !stats) return;
    fprintf(f, "AzmaFuzz statistics:\n");
    fprintf(f, "  runs        : %zu\n", stats->runs);
    fprintf(f, "  failures    : %zu\n", stats->failures);
    fprintf(f, "  generated   : %zu\n", stats->generated);
    fprintf(f, "  mutated     : %zu\n", stats->mutated);
    fprintf(f, "  bytes_tested: %zu\n", stats->bytes_tested);
}

AZMA_EXTERN_C_END

#endif /* AZMA_FUZZ_H */
