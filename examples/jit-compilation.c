#include <chomsky3.h>
#include <stdio.h>
#include <time.h>

int main(void) {
    const char *pattern = "\\b[A-Z][a-z]+\\b";  /* Match capitalized words */
    const char *text = "The Quick Brown Fox Jumps Over The Lazy Dog";
    
    /* Compile with JIT enabled */
    chomsky3_regex *regex = chomsky3_compile(pattern, CHOMSKY3_JIT);
    if (!regex) {
        fprintf(stderr, "Compilation failed\n");
        return 1;
    }
    
    /* Warm-up and benchmark */
    clock_t start = clock();
    int matches = 0;
    
    for (int i = 0; i < 100000; i++) {
        chomsky3_match *m = chomsky3_match(regex, text, strlen(text));
        if (m) matches++;
        chomsky3_match_free(m);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("Matched %d times in %.3f seconds\n", matches, elapsed);
    printf("Throughput: %.0f matches/sec\n", 100000.0 / elapsed);
    
    chomsky3_free(regex);
    return 0;
}
