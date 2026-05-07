#include <chomsky3.h>
#include <stdio.h>

int main(void) {
    /* Test various Unicode characters */
    uint32_t codepoints[] = {
        0x0041,  /* A */
        0x0020,  /* Space */
        0x03B1,  /* Greek alpha */
        0x1F600, /* Emoji: grinning face */
        0x200B   /* Zero-width space */
    };
    
    printf("Unicode Character Lookup:\n");
    printf("========================\n\n");
    
    for (int i = 0; i < 5; i++) {
        const char *name = chomsky3_unicode_name(codepoints[i]);
        
        if (name) {
            printf("U+%04X: %s\n", codepoints[i], name);
        } else {
            printf("U+%04X: (name not found)\n", codepoints[i]);
        }
    }
    
    printf("\n\nReverse Lookup:\n");
    printf("===============\n\n");
    
    const char *names[] = {
        "SPACE",
        "LATIN CAPITAL LETTER A",
        "GREEK SMALL LETTER ALPHA"
    };
    
    for (int i = 0; i < 3; i++) {
        uint32_t cp = chomsky3_unicode_codepoint(names[i]);
        
        if (cp != 0xFFFFFFFF) {
            printf("'%s' -> U+%04X\n", names[i], cp);
        } else {
            printf("'%s' -> (not found)\n", names[i]);
        }
    }
    
    return 0;
}
