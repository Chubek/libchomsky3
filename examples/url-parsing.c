#include <chomsky3.h>
#include <stdio.h>

int main(void) {
    const char *pattern = "^(https?)://([^/]+)(/.*)?$";
    const char *url = "https://github.com/user/repo";
    
    chomsky3_regex *regex = chomsky3_compile(pattern, 0);
    chomsky3_match *match = chomsky3_match(regex, url, strlen(url));
    
    if (match) {
        printf("Full match: %.*s\n", 
               (int)match->captures[0].length,
               url + match->captures[0].start);
        
        printf("Protocol: %.*s\n",
               (int)match->captures[1].length,
               url + match->captures[1].start);
        
        printf("Domain: %.*s\n",
               (int)match->captures[2].length,
               url + match->captures[2].start);
        
        if (match->num_captures > 3) {
            printf("Path: %.*s\n",
                   (int)match->captures[3].length,
                   url + match->captures[3].start);
        }
    }
    
    chomsky3_match_free(match);
    chomsky3_free(regex);
    return 0;
}
