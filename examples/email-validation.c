#include <chomsky3.h>
#include <stdio.h>

int main(void) {
    const char *pattern = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}";
    const char *emails[] = {
        "user@example.com",
        "invalid.email",
        "test+tag@domain.co.uk"
    };
    
    chomsky3_regex *regex = chomsky3_compile(pattern, 0);
    if (!regex) {
        fprintf(stderr, "Failed to compile pattern\n");
        return 1;
    }
    
    for (int i = 0; i < 3; i++) {
        chomsky3_match *match = chomsky3_match(regex, emails[i], strlen(emails[i]));
        printf("%s: %s\n", emails[i], match ? "VALID" : "INVALID");
        chomsky3_match_free(match);
    }
    
    chomsky3_free(regex);
    return 0;
}
