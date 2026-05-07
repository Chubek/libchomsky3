/**
 * libchomsky3 - Binary Code Deserialization Compatibility Unit
 */

#include "chomsky3/bincode.h"

/*
 * Deserialization entry points are implemented in bincode/format.c.
 * This translation unit exists to keep project structure stable and to
 * provide a single, non-empty TU for static analysis/build tooling.
 */

int chomsky3_bincode_deserialize_anchor(void) {
    return 0;
}
