// AFL fuzzer for grammar action evaluation and scope handling
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "chomsky3.h"

__AFL_FUZZ_INIT();

int main(void) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif
    
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        
        // Test with user parse node state
        D_Parser *parser = new_D_Parser(&expr_grammar_tables, sizeof(int));
        if (parser) {
            parser->save_parse_tree = 1;
            dparse(parser, (char*)buf, len);
            // Actions with $$.value, $0.value, ${scope} are exercised
            free_D_Parser(parser);
        }
    }
    
    return 0;
}
