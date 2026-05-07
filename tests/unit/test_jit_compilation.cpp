#include <catch2/catch_test_macros.hpp>
extern "C" {
#include "chomsky3.h"
#ifdef CHOMSKY3_HAS_JIT
#include "sljit/sljitLir.h"
#endif
}

TEST_CASE("JIT compilation", "[jit]") {
#ifdef CHOMSKY3_HAS_JIT
    SECTION("Create and free JIT compiler") {
        struct sljit_compiler *compiler = sljit_create_compiler(NULL, NULL);
        REQUIRE(compiler != nullptr);
        sljit_free_compiler(compiler);
    }
    
    SECTION("Generate simple code") {
        struct sljit_compiler *compiler = sljit_create_compiler(NULL, NULL);
        REQUIRE(compiler != nullptr);
        
        sljit_emit_enter(compiler, 0, SLJIT_ARGS0(W), 1, 1, 0, 0, 0);
        sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, 42);
        
        void *code = sljit_generate_code(compiler, 0, NULL);
        REQUIRE(code != nullptr);
        
        typedef int (*func_t)(void);
        func_t func = (func_t)code;
        REQUIRE(func() == 42);
        
        sljit_free_code(code, NULL);
        sljit_free_compiler(compiler);
    }
    
    SECTION("JIT with parser integration") {
        D_Parser *parser = new_D_Parser(&parser_tables_gram, 0);
        REQUIRE(parser != nullptr);
        
        // Test JIT-accelerated parsing
        const char *input = "test input";
        dparse(parser, input, strlen(input));
        
        free_D_Parser(parser);
    }
#else
    SKIP("JIT not enabled");
#endif
}
