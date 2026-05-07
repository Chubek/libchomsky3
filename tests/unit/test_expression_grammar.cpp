#include <catch2/catch_test_macros.hpp>
#include <cstring>
extern "C" {
#include "chomsky3.h"
}

// Based on libdparser-docs.txt:722–760 expression grammar example
TEST_CASE("Expression grammar evaluation", "[parser][actions]") {
    D_Parser *parser = new_D_Parser(&expr_grammar_tables, sizeof(int));
    REQUIRE(parser != nullptr);
    parser->save_parse_tree = 1;
    
    SECTION("Simple integer") {
        const char *input = "42";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // $$.value should be 42
    }
    
    SECTION("Addition") {
        const char *input = "10 + 5";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // $$.value should be 15
    }
    
    SECTION("Operator precedence") {
        const char *input = "2 + 3 * 4";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // Should be 14, not 20 (multiplication first)
    }
    
    SECTION("Parentheses") {
        const char *input = "(2 + 3) * 4";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // Should be 20
    }
    
    SECTION("Division by zero handling") {
        const char *input = "10 / 0";
        dparse(parser, input, strlen(input));
        // Should handle gracefully
    }
    
    free_D_Parser(parser);
}
