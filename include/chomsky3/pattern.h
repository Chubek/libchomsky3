#ifndef CHOMSKY3_PATTERN_H
#define CHOMSKY3_PATTERN_H

#include "chomsky3.h"

struct chomsky3_pattern {
    chomsky3_context_t *ctx;
    chomsky3_target_t target;
    chomsky3_flags_t flags;

    union {
        struct {
            void *code;
            size_t size;
            uint32_t num_registers;
        } bytecode;
        struct {
            void *code;
            size_t size;
            void *entry_point;
        } jit;
        struct {
            char *source;
            size_t length;
        } c_source;
    };

    size_t num_groups;
    size_t max_backtrack_depth;
};

#endif /* CHOMSKY3_PATTERN_H */
