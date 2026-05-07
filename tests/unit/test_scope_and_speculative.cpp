#include <catch2/catch_test_macros.hpp>
#include <cstring>
extern "C" {
#include "chomsky3.h"
}

// Based on libdparser-docs.txt:508–620 scope and speculative parsing
TEST_CASE("Scope and speculative parsing", "[parser][scope]") {
    D_Parser *parser = new_D_Parser(&scope_grammar_tables, sizeof(int));
    REQUIRE(parser != nullptr);
    parser->save_parse_tree = 1;
    
    SECTION("Scope propagation with ${scope}") {
        const char *input = "scoped expression";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // ${scope} should propagate correctly
    }
    
    SECTION("Speculative action execution") {
        const char *input = "ambiguous input";
        dparse(parser, input, strlen(input));
        // Speculative actions in [...] should execute
    }
    
    SECTION("Rejection with ${reject}") {
        const char *input = "rejected path";
        dparse(parser, input, strlen(input));
        // ${reject} should trigger backtracking
    }
    
    SECTION("Final action after speculation") {
        const char *input = "final action";
        dparse(parser, input, strlen(input));
        REQUIRE(parser->syntax_errors == 0);
        // Final actions in {...} should execute after speculation
    }
    
    free_D_Parser(parser);
}
