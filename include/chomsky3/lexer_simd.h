#ifndef CHOMSKY3_LEXER_SIMD_H
#define CHOMSKY3_LEXER_SIMD_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Detect a single contiguous byte range that loops back to this DFA state. */
bool chomsky3_lexer_simd_range(const int transitions[256], int state,
                              unsigned char *low, unsigned char *high);
#ifdef __cplusplus
}
#endif
#endif
