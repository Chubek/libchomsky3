// AFL fuzzer for JIT code generation paths
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "chomsky3.h"

#ifdef CHOMSKY3_HAS_JIT
#include "sljit/sljitLir.h"
#endif

__AFL_FUZZ_INIT();

int main(void) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif
    
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        
#ifdef CHOMSKY3_HAS_JIT
        struct sljit_compiler *compiler = sljit_create_compiler(NULL, NULL);
        if (compiler) {
            // Fuzz JIT compilation with arbitrary input patterns
            D_Parser *parser = new_D_Parser(&parser_tables_gram, 0);
            if (parser) {
                dparse(parser, (char*)buf, len);
                free_D_Parser(parser);
            }
            sljit_free_compiler(compiler);
        }
#endif
    }
    
    return 0;
}
