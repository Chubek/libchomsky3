/* src/unicode_names.c - Unicode name lookup for error reporting */
#include "chomsky3.h"
#include <string.h>

/* Include the generated table in exactly ONE translation unit */
#include "../third_party/unicode_names/unicode_names.h"

/* Binary search helper - table is sorted by codepoint */
const char* chomsky3_unicode_name(uint32_t codepoint) {
    size_t left = 0;
    size_t right = sizeof(unicode_name_table) / sizeof(unicode_name_table[0]) - 1;
    
    while (left <= right) {
        size_t mid = left + (right - left) / 2;
        
        if (unicode_name_table[mid].cp == codepoint) {
            return unicode_name_table[mid].name;
        }
        
        if (unicode_name_table[mid].cp < codepoint) {
            left = mid + 1;
        } else {
            if (mid == 0) break;
            right = mid - 1;
        }
    }
    
    return NULL;  /* Not found */
}

/* Optional: reverse lookup (name -> codepoint) */
uint32_t chomsky3_unicode_codepoint(const char *name) {
    size_t count = sizeof(unicode_name_table) / sizeof(unicode_name_table[0]);
    
    for (size_t i = 0; i < count; i++) {
        if (strcmp(unicode_name_table[i].name, name) == 0) {
            return unicode_name_table[i].cp;
        }
    }
    
    return 0xFFFFFFFF;  /* Invalid codepoint sentinel */
}
