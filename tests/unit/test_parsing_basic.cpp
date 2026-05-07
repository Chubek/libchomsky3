#include <catch2/catch_test_macros.hpp>
#include <cstring>
extern "C" {
#include "chomsky3.h"
}

TEST_CASE("Basic parsing", "[parser][parse]") {
    D_Parser *parser = new_D_Parser(&parser_tables_gram, 0);
    REQUIRE(parser != nullptr);
    
    SECTION("Valid input succeeds") {
        const char *input = "valid expression";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
    }
    
    SECTION("Invalid input fails") {
        const char *input = "invalid @@@ syntax";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors > 0);
    }
    
    SECTION("Empty input") {
        const char *input = "";
        dparse(parser, input, 0);
        // Behavior depends on grammar
    }
    
    SECTION("Null-terminated vs length-based") {
        const char *input = "test\0hidden";
        dparse(parser, input, 4);
        // Should only parse "test"
    }
    
    free_D_Parser(parser);
}
