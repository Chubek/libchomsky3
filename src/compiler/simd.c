#include <stdbool.h>
#include "chomsky3/lexer_simd.h"

bool chomsky3_lexer_simd_range(const int transitions[256], int state,
                              unsigned char *low, unsigned char *high) {
    int first = -1, last = -1;
    bool gap = false;
    if (!transitions || !low || !high) return false;
    for (int c = 0; c < 256; ++c) {
        if (transitions[c] == state) {
            if (gap) return false;
            if (first < 0) first = c;
            last = c;
        } else if (first >= 0) gap = true;
    }
    if (first < 0) return false;
    *low = (unsigned char)first;
    *high = (unsigned char)last;
    return true;
}
