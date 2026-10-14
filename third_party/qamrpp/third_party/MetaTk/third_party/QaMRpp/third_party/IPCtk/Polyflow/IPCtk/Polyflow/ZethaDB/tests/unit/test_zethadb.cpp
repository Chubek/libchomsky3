#include "../../ZethaMEM.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parse schema accepts valid definition") {
    auto schema = zethamem::parse_schema("table users { id: int; name: string; active: bool; }");
    REQUIRE(schema.table_name == "users");
    REQUIRE(schema.columns.size() == 3);
}

TEST_CASE("parse schema rejects duplicate columns") {
    REQUIRE_THROWS_AS(
        zethamem::parse_schema("table users { id: int; id: int; }"),
        zethamem::Error);
}

TEST_CASE("parse schema rejects unknown type") {
    REQUIRE_THROWS_AS(
        zethamem::parse_schema("table users { id: uuid; }"),
        zethamem::Error);
}

TEST_CASE("parse schema rejects malformed punctuation") {
    REQUIRE_THROWS_AS(
        zethamem::parse_schema("table users { id int; }"),
        zethamem::Error);
}

TEST_CASE("parse insert query") {
    auto q = zethamem::parse_query("insert users { id: 1, name: \"alice\" }");
    auto* iq = std::get_if<zethamem::InsertQuery>(&q);
    REQUIRE(iq != nullptr);
    REQUIRE(iq->table == "users");
    REQUIRE(iq->values.size() == 2);
}

TEST_CASE("parse select where query") {
    auto q = zethamem::parse_query("select users where id >= 3");
    auto* sq = std::get_if<zethamem::SelectQuery>(&q);
    REQUIRE(sq != nullptr);
    REQUIRE(sq->where.has_value());
}

TEST_CASE("parse update query") {
    auto q = zethamem::parse_query("update users set active = false where id == 3");
    auto* uq = std::get_if<zethamem::UpdateQuery>(&q);
    REQUIRE(uq != nullptr);
    REQUIRE(uq->assignments.size() == 1);
    REQUIRE(uq->where.has_value());
}

TEST_CASE("parse delete query") {
    auto q = zethamem::parse_query("delete users where id == 2");
    auto* dq = std::get_if<zethamem::DeleteQuery>(&q);
    REQUIRE(dq != nullptr);
    REQUIRE(dq->where.has_value());
}

TEST_CASE("runtime insert then select") {
    zethamem::Database db;
    zethamem::exec_schema(db, "table users { id: int; name: string; }");
    zethamem::exec_query(db, "insert users { id: 1, name: \"alice\" }");
    auto result = zethamem::exec_query(db, "select users");
    REQUIRE(result.rows.size() == 1);
}

TEST_CASE("runtime update then select") {
    zethamem::Database db;
    zethamem::exec_schema(db, "table users { id: int; name: string; }");
    zethamem::exec_query(db, "insert users { id: 1, name: \"alice\" }");
    auto updated = zethamem::exec_query(db, "update users set name = \"bob\" where id == 1");
    REQUIRE(updated.affected == 1);
    auto selected = zethamem::exec_query(db, "select users where id == 1");
    REQUIRE(std::get<std::string>(selected.rows[0][1]) == "bob");
}

TEST_CASE("runtime delete then select") {
    zethamem::Database db;
    zethamem::exec_schema(db, "table users { id: int; }");
    zethamem::exec_query(db, "insert users { id: 1 }");
    auto deleted = zethamem::exec_query(db, "delete users where id == 1");
    REQUIRE(deleted.affected == 1);
    auto selected = zethamem::exec_query(db, "select users");
    REQUIRE(selected.rows.empty());
}

TEST_CASE("runtime rejects missing field") {
    zethamem::Database db;
    zethamem::exec_schema(db, "table users { id: int; name: string; }");
    REQUIRE_THROWS_AS(
        zethamem::exec_query(db, "insert users { id: 1 }"),
        zethamem::Error);
}

TEST_CASE("whitespace-heavy input parses") {
    zethamem::Database db;
    zethamem::exec_schema(db, " table users  {  id: int;  active: bool; } ");
    zethamem::exec_query(db, " insert users { id: 1, active: true } ");
    auto result = zethamem::exec_query(db, " select users where id == 1 ");
    REQUIRE(result.rows.size() == 1);
}
