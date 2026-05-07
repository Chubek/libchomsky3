#include <catch2/catch_test_macros.hpp>
extern "C" {
#include "chomsky3.h"
}

TEST_CASE("Parser lifecycle", "[parser][core]") {
    SECTION("Create and destroy parser") {
        D_Parser *parser = new_D_Parser(&parser_tables_gram, 0);
        REQUIRE(parser != nullptr);
        free_D_Parser(parser);
    }
    
    SECTION("Create parser with user parse node size") {
        D_Parser *parser = new_D_Parser(&parser_tables_gram, sizeof(int));
        REQUIRE(parser != nullptr);
        REQUIRE(parser->sizeof_user_parse_node == sizeof(int));
        free_D_Parser(parser);
    }
    
    SECTION("Multiple parser instances") {
        D_Parser *p1 = new_D_Parser(&parser_tables_gram, 0);
        D_Parser *p2 = new_D_Parser(&parser_tables_gram, 0);
        REQUIRE(p1 != nullptr);
        REQUIRE(p2 != nullptr);
        REQUIRE(p1 != p2);
        free_D_Parser(p1);
        free_D_Parser(p2);
    }
}
