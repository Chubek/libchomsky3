#include <chomsky3.h>
#include <stdio.h>

int main(void) {
    /* Pathological pattern vulnerable to catastrophic backtracking */
    const char *pattern = "(a+)+b";
    const char *input = "aaaaaaaaaaaaaaaaaaaaaaaaaaac";  /* No 'b' at end */
    
    chomsky3_regex *regex = chomsky3_compile(pattern, 0);
    
    /* Configure strict resource limits */
    chomsky3_config config = {
        .max_steps = 10000,        /* Limit execution steps */
        .max_backtracks = 1000,    /* Limit backtracking */
        .max_stack_depth = 100     /* Limit recursion */
    };
    
    chomsky3_match *match = chomsky3_match_ex(regex, input, strlen(input), &config);
    
    if (!match) {
        chomsky3_error *err = chomsky3_get_last_error();
        if (err->code == CHOMSKY3_ERR_RESOURCE_LIMIT) {
            printf("Pattern execution exceeded resource limits (ReDoS protection)\n");
            printf("Error: %s\n", err->message);
        }
    } else {
        printf("Match found\n");
        chomsky3_match_free(match);
    }
    
    chomsky3_free(regex);
    return 0;
}
