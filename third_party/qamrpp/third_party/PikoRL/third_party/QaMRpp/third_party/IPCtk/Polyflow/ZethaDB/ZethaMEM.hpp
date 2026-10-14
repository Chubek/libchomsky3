#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../PolyflowDSLUtils.hpp"
#include "ZethaQUERY.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace zethamem {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class ValueType {
    Int,
    Double,
    Bool,
    String
};

inline std::string to_string(ValueType t) {
    switch (t) {
        case ValueType::Int: return "int";
        case ValueType::Double: return "double";
        case ValueType::Bool: return "bool";
        case ValueType::String: return "string";
    }
    return "unknown";
}

using Value = std::variant<std::int64_t, double, bool, std::string>;

inline ValueType value_type_of(const Value& v) {
    if (std::holds_alternative<std::int64_t>(v)) return ValueType::Int;
    if (std::holds_alternative<double>(v)) return ValueType::Double;
    if (std::holds_alternative<bool>(v)) return ValueType::Bool;
    return ValueType::String;
}

inline std::string value_to_string(const Value& v) {
    struct Visitor {
        std::string operator()(std::int64_t x) const { return std::to_string(x); }
        std::string operator()(double x) const {
            std::ostringstream out;
            out << x;
            return out.str();
        }
        std::string operator()(bool x) const { return x ? "true" : "false"; }
        std::string operator()(const std::string& x) const { return x; }
    };
    return std::visit(Visitor{}, v);
}

inline bool value_equals(const Value& a, const Value& b) {
    if (a.index() == b.index()) return a == b;

    auto is_num = [](const Value& v) {
        return std::holds_alternative<std::int64_t>(v) || std::holds_alternative<double>(v);
    };

    if (is_num(a) && is_num(b)) {
        const double da = std::holds_alternative<std::int64_t>(a)
            ? static_cast<double>(std::get<std::int64_t>(a))
            : std::get<double>(a);
        const double db = std::holds_alternative<std::int64_t>(b)
            ? static_cast<double>(std::get<std::int64_t>(b))
            : std::get<double>(b);
        return da == db;
    }

    return false;
}

inline int value_compare(const Value& a, const Value& b) {
    auto is_num = [](const Value& v) {
        return std::holds_alternative<std::int64_t>(v) || std::holds_alternative<double>(v);
    };

    if (is_num(a) && is_num(b)) {
        const double da = std::holds_alternative<std::int64_t>(a)
            ? static_cast<double>(std::get<std::int64_t>(a))
            : std::get<double>(a);
        const double db = std::holds_alternative<std::int64_t>(b)
            ? static_cast<double>(std::get<std::int64_t>(b))
            : std::get<double>(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }

    if (a.index() != b.index()) {
        throw Error("cannot compare values of different non-numeric types");
    }

    if (std::holds_alternative<bool>(a)) {
        const bool av = std::get<bool>(a);
        const bool bv = std::get<bool>(b);
        if (av == bv) return 0;
        return av ? 1 : -1;
    }

    if (std::holds_alternative<std::string>(a)) {
        const auto& av = std::get<std::string>(a);
        const auto& bv = std::get<std::string>(b);
        if (av < bv) return -1;
        if (av > bv) return 1;
        return 0;
    }

    throw Error("unsupported comparison");
}

inline Value parse_typed_value(ValueType t, const Value& input) {
    switch (t) {
        case ValueType::Int:
            if (std::holds_alternative<std::int64_t>(input)) return input;
            if (std::holds_alternative<double>(input)) return static_cast<std::int64_t>(std::get<double>(input));
            break;
        case ValueType::Double:
            if (std::holds_alternative<double>(input)) return input;
            if (std::holds_alternative<std::int64_t>(input)) return static_cast<double>(std::get<std::int64_t>(input));
            break;
        case ValueType::Bool:
            if (std::holds_alternative<bool>(input)) return input;
            break;
        case ValueType::String:
            if (std::holds_alternative<std::string>(input)) return input;
            break;
    }

    throw Error("type mismatch: expected " + to_string(t) + ", got " + to_string(value_type_of(input)));
}

struct Column {
    std::string name;
    ValueType type;
};

struct Schema {
    std::string table_name;
    std::vector<Column> columns;
    std::unordered_map<std::string, std::size_t> index_by_name;

    void rebuild_index() {
        index_by_name.clear();
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (index_by_name.find(columns[i].name) != index_by_name.end()) {
                throw Error("duplicate column: " + columns[i].name);
            }
            index_by_name[columns[i].name] = i;
        }
    }

    std::size_t column_index(const std::string& name) const {
        auto it = index_by_name.find(name);
        if (it == index_by_name.end()) throw Error("unknown column: " + name);
        return it->second;
    }

    const Column& column(const std::string& name) const {
        return columns[column_index(name)];
    }
};

using Row = std::vector<Value>;

enum class CompareOp {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge
};

struct Predicate {
    std::string column;
    CompareOp op;
    Value value;
};

struct InsertQuery {
    std::string table;
    std::vector<std::pair<std::string, Value>> values;
};

struct SelectQuery {
    std::string table;
    std::optional<Predicate> where;
};

struct DeleteQuery {
    std::string table;
    std::optional<Predicate> where;
};

struct UpdateQuery {
    std::string table;
    std::vector<std::pair<std::string, Value>> assignments;
    std::optional<Predicate> where;
};

using Query = std::variant<InsertQuery, SelectQuery, DeleteQuery, UpdateQuery>;

struct QueryResult {
    std::vector<std::string> columns;
    std::vector<Row> rows;
    std::size_t affected = 0;
};

class Table {
public:
    Table() = default;
    explicit Table(Schema schema) : schema_(std::move(schema)) { schema_.rebuild_index(); }

    const Schema& schema() const { return schema_; }
    const std::vector<Row>& rows() const { return rows_; }

    void insert(const std::vector<std::pair<std::string, Value>>& fields) {
        Row row(schema_.columns.size());
        std::vector<bool> assigned(schema_.columns.size(), false);

        for (const auto& [name, value] : fields) {
            std::size_t idx = schema_.column_index(name);
            row[idx] = parse_typed_value(schema_.columns[idx].type, value);
            assigned[idx] = true;
        }

        for (std::size_t i = 0; i < schema_.columns.size(); ++i) {
            if (!assigned[i]) throw Error("missing column in insert: " + schema_.columns[i].name);
        }

        rows_.push_back(std::move(row));
    }

    QueryResult select(std::optional<Predicate> pred = std::nullopt) const {
        QueryResult out;
        for (const auto& c : schema_.columns) out.columns.push_back(c.name);
        for (const auto& row : rows_) {
            if (!pred || matches(row, *pred)) out.rows.push_back(row);
        }
        out.affected = out.rows.size();
        return out;
    }

    std::size_t erase(std::optional<Predicate> pred = std::nullopt) {
        const std::size_t old_size = rows_.size();
        rows_.erase(
            std::remove_if(rows_.begin(), rows_.end(), [&](const Row& r) {
                return !pred || matches(r, *pred);
            }),
            rows_.end());
        return old_size - rows_.size();
    }

    std::size_t update(const std::vector<std::pair<std::string, Value>>& assignments,
                       std::optional<Predicate> pred = std::nullopt) {
        std::size_t count = 0;
        for (auto& row : rows_) {
            if (pred && !matches(row, *pred)) continue;

            for (const auto& [name, value] : assignments) {
                std::size_t idx = schema_.column_index(name);
                row[idx] = parse_typed_value(schema_.columns[idx].type, value);
            }
            ++count;
        }
        return count;
    }

private:
    bool matches(const Row& row, const Predicate& pred) const {
        const std::size_t idx = schema_.column_index(pred.column);
        const Value& lhs = row[idx];
        const Value rhs = parse_typed_value(schema_.columns[idx].type, pred.value);

        switch (pred.op) {
            case CompareOp::Eq: return value_equals(lhs, rhs);
            case CompareOp::Ne: return !value_equals(lhs, rhs);
            case CompareOp::Lt: return value_compare(lhs, rhs) < 0;
            case CompareOp::Le: return value_compare(lhs, rhs) <= 0;
            case CompareOp::Gt: return value_compare(lhs, rhs) > 0;
            case CompareOp::Ge: return value_compare(lhs, rhs) >= 0;
        }
        return false;
    }

    Schema schema_;
    std::vector<Row> rows_;
};

class Database {
public:
    bool has_table(const std::string& name) const {
        return tables_.find(name) != tables_.end();
    }

    void create_table(const Schema& schema) {
        if (schema.table_name.empty()) throw Error("table name cannot be empty");
        if (has_table(schema.table_name)) throw Error("table already exists: " + schema.table_name);

        Schema s = schema;
        s.rebuild_index();
        const std::string table_name = s.table_name;
        tables_.emplace(table_name, Table(std::move(s)));
    }

    Table& table(const std::string& name) {
        auto it = tables_.find(name);
        if (it == tables_.end()) throw Error("unknown table: " + name);
        return it->second;
    }

    const Table& table(const std::string& name) const {
        auto it = tables_.find(name);
        if (it == tables_.end()) throw Error("unknown table: " + name);
        return it->second;
    }

    QueryResult execute(const Query& q) {
        return std::visit([this](const auto& qq) { return execute_one(qq); }, q);
    }

private:
    QueryResult execute_one(const InsertQuery& q) {
        table(q.table).insert(q.values);
        QueryResult r;
        r.affected = 1;
        return r;
    }

    QueryResult execute_one(const SelectQuery& q) {
        return table(q.table).select(q.where);
    }

    QueryResult execute_one(const DeleteQuery& q) {
        QueryResult r;
        r.affected = table(q.table).erase(q.where);
        return r;
    }

    QueryResult execute_one(const UpdateQuery& q) {
        QueryResult r;
        r.affected = table(q.table).update(q.assignments, q.where);
        return r;
    }

    std::unordered_map<std::string, Table> tables_;
};

namespace detail {

template <typename T, typename Fn>
inline dsl::Parser<T> parser_opt(Fn&& fn) {
    return dsl::parser([f = std::forward<Fn>(fn)](dsl::ParsecInput& in) -> dsl::ExpectedResult<T> {
        if (auto parsed = f(in)) return dsl::ExpectedResult<T>{std::move(*parsed)};
        return dsl::ExpectedResult<T>{std::nullopt};
    });
}

inline void skip_ws(dsl::ParsecInput& in) {
    while (!in.eof() && std::isspace(static_cast<unsigned char>(in.peek()))) {
        in.consume();
    }
}

inline dsl::Parser<char> symbol(char c) {
    return parser_opt<char>([c](dsl::ParsecInput& in) -> std::optional<char> {
        skip_ws(in);
        if (!in.eof() && in.peek() == c) {
            return in.consume();
        }
        return std::nullopt;
    });
}

inline dsl::Parser<std::string> identifier() {
    return parser_opt<std::string>([](dsl::ParsecInput& in) -> std::optional<std::string> {
        skip_ws(in);
        if (in.eof()) return std::nullopt;
        const char first = in.peek();
        if (!(std::isalpha(static_cast<unsigned char>(first)) || first == '_')) return std::nullopt;

        std::string out;
        out.push_back(in.consume());
        while (!in.eof()) {
            const char c = in.peek();
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                out.push_back(in.consume());
            } else {
                break;
            }
        }
        return out;
    });
}

inline dsl::Parser<std::string> keyword(const std::string& kw) {
    return parser_opt<std::string>([kw](dsl::ParsecInput& in) -> std::optional<std::string> {
        skip_ws(in);
        const std::size_t save = in.pos;
        for (char c : kw) {
            if (in.eof() || in.peek() != c) {
                in.pos = save;
                return std::nullopt;
            }
            in.consume();
        }

        if (!in.eof()) {
            const char next = in.peek();
            if (std::isalnum(static_cast<unsigned char>(next)) || next == '_') {
                in.pos = save;
                return std::nullopt;
            }
        }

        return kw;
    });
}

inline dsl::Parser<std::string> string_literal() {
    return parser_opt<std::string>([](dsl::ParsecInput& in) -> std::optional<std::string> {
        skip_ws(in);
        if (in.eof() || in.peek() != '"') return std::nullopt;
        in.consume();

        std::string out;
        while (!in.eof()) {
            const char c = in.consume();
            if (c == '"') return out;
            if (c == '\\') {
                if (in.eof()) return std::nullopt;
                const char e = in.consume();
                switch (e) {
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default: out.push_back(e); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return std::nullopt;
    });
}

inline dsl::Parser<std::int64_t> integer_literal() {
    return parser_opt<std::int64_t>([](dsl::ParsecInput& in) -> std::optional<std::int64_t> {
        skip_ws(in);
        const std::size_t save = in.pos;

        std::string text;
        if (!in.eof() && in.peek() == '-') text.push_back(in.consume());
        if (in.eof() || !std::isdigit(static_cast<unsigned char>(in.peek()))) {
            in.pos = save;
            return std::nullopt;
        }

        while (!in.eof() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
            text.push_back(in.consume());
        }

        return static_cast<std::int64_t>(std::strtoll(text.c_str(), nullptr, 10));
    });
}

inline dsl::Parser<double> float_literal() {
    return parser_opt<double>([](dsl::ParsecInput& in) -> std::optional<double> {
        skip_ws(in);
        const std::size_t save = in.pos;

        std::string text;
        if (!in.eof() && in.peek() == '-') text.push_back(in.consume());
        if (in.eof() || !std::isdigit(static_cast<unsigned char>(in.peek()))) {
            in.pos = save;
            return std::nullopt;
        }

        while (!in.eof() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
            text.push_back(in.consume());
        }

        if (in.eof() || in.peek() != '.') {
            in.pos = save;
            return std::nullopt;
        }
        text.push_back(in.consume());

        if (in.eof() || !std::isdigit(static_cast<unsigned char>(in.peek()))) {
            in.pos = save;
            return std::nullopt;
        }

        while (!in.eof() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
            text.push_back(in.consume());
        }

        return std::strtod(text.c_str(), nullptr);
    });
}

inline dsl::Parser<bool> bool_literal() {
    return parser_opt<bool>([](dsl::ParsecInput& in) -> std::optional<bool> {
        const std::size_t save = in.pos;
        if (keyword("true")(in)) return true;
        in.pos = save;
        if (keyword("false")(in)) return false;
        return std::nullopt;
    });
}

inline dsl::Parser<Value> value_literal() {
    return parser_opt<Value>([](dsl::ParsecInput& in) -> std::optional<Value> {
        const std::size_t save = in.pos;
        const auto try_parse = [&in, save](const auto& parser, auto map_fn) -> std::optional<Value> {
            in.pos = save;
            if (auto parsed = parser(in)) return map_fn(*parsed);
            return std::nullopt;
        };
        if (auto s = try_parse(string_literal(), [](const std::string& v) { return Value{v}; })) return s;
        if (auto d = try_parse(float_literal(), [](double v) { return Value{v}; })) return d;
        if (auto i = try_parse(integer_literal(), [](std::int64_t v) { return Value{v}; })) return i;
        if (auto b = try_parse(bool_literal(), [](bool v) { return Value{v}; })) return b;
        in.pos = save;
        return std::nullopt;
    });
}

inline ValueType parse_type_name(const std::string& s) {
    if (s == "int") return ValueType::Int;
    if (s == "double") return ValueType::Double;
    if (s == "bool") return ValueType::Bool;
    if (s == "string") return ValueType::String;
    throw Error("unknown type: " + s);
}

inline dsl::Parser<CompareOp> comparison_op() {
    return parser_opt<CompareOp>([](dsl::ParsecInput& in) -> std::optional<CompareOp> {
        const std::size_t save = in.pos;
        const auto parse_token = [&in, save](std::string_view token, CompareOp op) -> std::optional<CompareOp> {
            in.pos = save;
            if (keyword(std::string(token))(in)) return op;
            return std::nullopt;
        };
        if (auto op = parse_token("==", CompareOp::Eq)) return op;
        if (auto op = parse_token("!=", CompareOp::Ne)) return op;
        if (auto op = parse_token("<=", CompareOp::Le)) return op;
        if (auto op = parse_token(">=", CompareOp::Ge)) return op;
        if (auto op = parse_token("<", CompareOp::Lt)) return op;
        if (auto op = parse_token(">", CompareOp::Gt)) return op;
        in.pos = save;
        return std::nullopt;
    });
}

inline dsl::Parser<Predicate> predicate() {
    return parser_opt<Predicate>([](dsl::ParsecInput& in) -> std::optional<Predicate> {
        auto id = identifier()(in);
        if (!id) return std::nullopt;
        auto op = comparison_op()(in);
        if (!op) return std::nullopt;
        auto value = value_literal()(in);
        if (!value) return std::nullopt;
        return Predicate{*id, *op, *value};
    });
}

inline void expect_end(dsl::ParsecInput& in, const char* context) {
    skip_ws(in);
    if (!in.eof()) {
        throw Error(std::string("unexpected trailing input in ") + context);
    }
}

} // namespace detail

inline Schema parse_schema(std::string_view text) {
    dsl::ParsecInput in{text, 0};

    if (!detail::keyword("table")(in)) {
        throw Error("schema must start with 'table'");
    }

    auto table = detail::identifier()(in);
    if (!table) throw Error("expected table name");

    if (!detail::symbol('{')(in)) throw Error("expected '{' after table name");

    Schema schema;
    schema.table_name = *table;

    while (true) {
        detail::skip_ws(in);
        if (detail::symbol('}')(in)) break;

        auto col_name = detail::identifier()(in);
        if (!col_name) throw Error("expected column name");
        if (!detail::symbol(':')(in)) throw Error("expected ':' after column name");

        auto type_name = detail::identifier()(in);
        if (!type_name) throw Error("expected column type");

        Column col;
        col.name = *col_name;
        col.type = detail::parse_type_name(*type_name);

        if (!detail::symbol(';')(in)) throw Error("expected ';' after column declaration");
        schema.columns.push_back(std::move(col));
    }

    schema.rebuild_index();
    detail::expect_end(in, "schema");
    return schema;
}

inline Query parse_query(std::string_view text) {
    dsl::ParsecInput in{text, 0};

    if (detail::keyword("insert")(in)) {
        InsertQuery q;
        auto table = detail::identifier()(in);
        if (!table) throw Error("expected table name after insert");
        q.table = *table;

        if (!detail::symbol('{')(in)) throw Error("expected '{' in insert");

        while (true) {
            detail::skip_ws(in);
            if (detail::symbol('}')(in)) break;

            auto name = detail::identifier()(in);
            if (!name) throw Error("expected column name in insert");
            if (!detail::symbol(':')(in)) throw Error("expected ':' in insert assignment");
            auto value = detail::value_literal()(in);
            if (!value) throw Error("expected literal in insert assignment");

            q.values.push_back({*name, *value});

            detail::skip_ws(in);
            if (detail::symbol(',')(in)) continue;
            if (detail::symbol('}')(in)) break;
            throw Error("expected ',' or '}' in insert");
        }

        detail::expect_end(in, "insert query");
        return q;
    }

    if (detail::keyword("select")(in)) {
        SelectQuery q;
        auto table = detail::identifier()(in);
        if (!table) throw Error("expected table name after select");
        q.table = *table;

        const std::size_t save = in.pos;
        if (detail::keyword("where")(in)) {
            auto pred = detail::predicate()(in);
            if (!pred) throw Error("expected predicate after where");
            q.where = *pred;
        } else {
            in.pos = save;
        }

        detail::expect_end(in, "select query");
        return q;
    }

    if (detail::keyword("update")(in)) {
        UpdateQuery q;
        auto table = detail::identifier()(in);
        if (!table) throw Error("expected table name after update");
        q.table = *table;

        if (!detail::keyword("set")(in)) throw Error("expected 'set' in update query");

        while (true) {
            auto name = detail::identifier()(in);
            if (!name) throw Error("expected column name in update assignment");
            if (!detail::symbol('=')(in)) throw Error("expected '=' in update assignment");
            auto value = detail::value_literal()(in);
            if (!value) throw Error("expected literal value in update assignment");

            q.assignments.push_back({*name, *value});

            detail::skip_ws(in);
            if (detail::symbol(',')(in)) continue;
            break;
        }

        const std::size_t save = in.pos;
        if (detail::keyword("where")(in)) {
            auto pred = detail::predicate()(in);
            if (!pred) throw Error("expected predicate after where");
            q.where = *pred;
        } else {
            in.pos = save;
        }

        detail::expect_end(in, "update query");
        return q;
    }

    if (detail::keyword("delete")(in)) {
        DeleteQuery q;
        auto table = detail::identifier()(in);
        if (!table) throw Error("expected table name after delete");
        q.table = *table;

        const std::size_t save = in.pos;
        if (detail::keyword("where")(in)) {
            auto pred = detail::predicate()(in);
            if (!pred) throw Error("expected predicate after where");
            q.where = *pred;
        } else {
            in.pos = save;
        }

        detail::expect_end(in, "delete query");
        return q;
    }

    throw Error("unknown query");
}

inline void exec_schema(Database& db, std::string_view schema_text) {
    db.create_table(parse_schema(schema_text));
}

inline QueryResult exec_query(Database& db, std::string_view query_text) {
    return db.execute(parse_query(query_text));
}

namespace memv2 {

using ParseResultError = dsl::ParseError;
template <typename T>
using Result = dsl::Result<T, ParseResultError>;
using Status = Result<std::monostate>;

inline ParseResultError mem_error(std::size_t pos, std::string expected) {
    return ParseResultError{pos, dsl::ParseFailureKind::Committed, {std::move(expected)}};
}

struct Record {
    std::map<std::string, Value> fields;
};

struct Snapshot {
    std::map<std::string, Record> entries;
};

namespace serde {

inline std::string esc(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '\n' || c == '\t' || c == '|') out.push_back('\\');
        out.push_back(c == '\n' ? 'n' : (c == '\t' ? 't' : c));
    }
    return out;
}

inline Result<std::string> unesc(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\') { out.push_back(s[i]); continue; }
        if (i + 1 >= s.size()) return Result<std::string>::from_err(mem_error(i, "escape"));
        ++i;
        out.push_back(s[i] == 'n' ? '\n' : (s[i] == 't' ? '\t' : s[i]));
    }
    return Result<std::string>::from_ok(std::move(out));
}

inline Result<std::vector<std::uint8_t>> encode(const Snapshot& s) {
    std::string out = "ZMEMSERDE1\n";
    for (const auto& kv : s.entries) {
        out += "P|" + esc(kv.first) + "\n";
        for (const auto& f : kv.second.fields) {
            const std::string key = esc(f.first);
            if (std::holds_alternative<std::int64_t>(f.second)) {
                out += "F|" + key + "|i|" + std::to_string(std::get<std::int64_t>(f.second)) + "\n";
            } else if (std::holds_alternative<double>(f.second)) {
                out += "F|" + key + "|d|" + value_to_string(f.second) + "\n";
            } else if (std::holds_alternative<bool>(f.second)) {
                out += "F|" + key + "|b|" + std::string(std::get<bool>(f.second) ? "1" : "0") + "\n";
            } else {
                out += "F|" + key + "|s|" + esc(std::get<std::string>(f.second)) + "\n";
            }
        }
        out += "E\n";
    }
    return Result<std::vector<std::uint8_t>>::from_ok(std::vector<std::uint8_t>(out.begin(), out.end()));
}

inline Result<Snapshot> decode(const std::vector<std::uint8_t>& bytes) {
    const std::string text(bytes.begin(), bytes.end());
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "ZMEMSERDE1") return Result<Snapshot>::from_err(mem_error(0, "serde magic"));
    Snapshot snap;
    std::string current_path;
    Record current;
    bool in_record = false;
    std::size_t ln = 1;
    while (std::getline(in, line)) {
        ++ln;
        if (line.empty()) continue;
        if (line == "E") {
            if (!in_record) return Result<Snapshot>::from_err(mem_error(ln, "end-record"));
            snap.entries[current_path] = current;
            current = Record{};
            current_path.clear();
            in_record = false;
            continue;
        }
        if (line.rfind("P|", 0) == 0) {
            if (in_record) return Result<Snapshot>::from_err(mem_error(ln, "missing E"));
            auto p = unesc(std::string_view(line).substr(2));
            if (p.is_err()) return Result<Snapshot>::from_err(mem_error(ln, "path escape"));
            current_path = p.unwrap();
            in_record = true;
            continue;
        }
        if (line.rfind("F|", 0) == 0) {
            if (!in_record) return Result<Snapshot>::from_err(mem_error(ln, "field outside record"));
            std::vector<std::string_view> tok;
            std::string_view sv(line);
            std::size_t start = 0;
            while (start <= sv.size()) {
                const auto pos = sv.find('|', start);
                if (pos == std::string_view::npos) { tok.push_back(sv.substr(start)); break; }
                tok.push_back(sv.substr(start, pos - start));
                start = pos + 1;
            }
            if (tok.size() != 4) return Result<Snapshot>::from_err(mem_error(ln, "field format"));
            auto key = unesc(tok[1]); if (key.is_err()) return Result<Snapshot>::from_err(mem_error(ln, "field key"));
            if (tok[2] == "i") current.fields[key.unwrap()] = static_cast<std::int64_t>(std::strtoll(std::string(tok[3]).c_str(), nullptr, 10));
            else if (tok[2] == "d") current.fields[key.unwrap()] = std::strtod(std::string(tok[3]).c_str(), nullptr);
            else if (tok[2] == "b") current.fields[key.unwrap()] = (tok[3] == "1");
            else if (tok[2] == "s") {
                auto val = unesc(tok[3]); if (val.is_err()) return Result<Snapshot>::from_err(mem_error(ln, "field value"));
                current.fields[key.unwrap()] = val.unwrap();
            } else return Result<Snapshot>::from_err(mem_error(ln, "field type"));
            continue;
        }
        return Result<Snapshot>::from_err(mem_error(ln, "unknown serde line"));
    }
    if (in_record) return Result<Snapshot>::from_err(mem_error(ln, "unterminated record"));
    return Result<Snapshot>::from_ok(std::move(snap));
}

} // namespace serde

class MMapRegion {
public:
    MMapRegion() = default;
    ~MMapRegion() { close(); }
    MMapRegion(const MMapRegion&) = delete;
    MMapRegion& operator=(const MMapRegion&) = delete;
    MMapRegion(MMapRegion&& o) noexcept { *this = std::move(o); }
    MMapRegion& operator=(MMapRegion&& o) noexcept {
        if (this == &o) return *this;
        close();
        fd_ = o.fd_; base_ = o.base_; len_ = o.len_;
        o.fd_ = -1; o.base_ = nullptr; o.len_ = 0;
        return *this;
    }

    Status open_or_create(const std::string& path, std::size_t len) {
#if defined(_WIN32)
        (void)path; (void)len;
        return Status::from_err(mem_error(0, "mmap mode unsupported on windows scaffold"));
#else
        if (len == 0) return Status::from_err(mem_error(0, "mmap length"));
        close();
        fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd_ < 0) return Status::from_err(mem_error(0, "open"));
        if (::ftruncate(fd_, static_cast<off_t>(len)) != 0) {
            close();
            return Status::from_err(mem_error(0, "ftruncate"));
        }
        void* p = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (p == MAP_FAILED) {
            close();
            return Status::from_err(mem_error(0, "mmap"));
        }
        base_ = reinterpret_cast<std::uint8_t*>(p);
        len_ = len;
        return Status::from_ok(std::monostate{});
#endif
    }

    Status sync() {
#if defined(_WIN32)
        return Status::from_ok(std::monostate{});
#else
        if (!base_ || len_ == 0) return Status::from_ok(std::monostate{});
        if (::msync(base_, len_, MS_SYNC) != 0) return Status::from_err(mem_error(0, "msync"));
        return Status::from_ok(std::monostate{});
#endif
    }

    void close() {
#if !defined(_WIN32)
        if (base_ && len_ > 0) ::munmap(base_, len_);
        if (fd_ >= 0) ::close(fd_);
#endif
        fd_ = -1;
        base_ = nullptr;
        len_ = 0;
    }

    std::uint8_t* data() { return base_; }
    const std::uint8_t* data() const { return base_; }
    std::size_t size() const { return len_; }
    bool mapped() const { return base_ != nullptr; }

private:
    int fd_ = -1;
    std::uint8_t* base_ = nullptr;
    std::size_t len_ = 0;
};

enum class Mode {
    PureRAM,
    MMapBacked
};

class MemDB {
public:
    explicit MemDB(Mode m = Mode::PureRAM) : mode_(m) {}

    Status open_mmap(const std::string& file, std::size_t map_len = 1 << 20) {
        mode_ = Mode::MMapBacked;
        mmap_path_ = file;
        auto mm = mmap_.open_or_create(file, map_len);
        if (mm.is_err()) return Status::from_err(mem_error(0, "mmap open"));
        if (mmap_.size() >= 8 && std::memcmp(mmap_.data(), "ZMEMv2\0", 7) == 0) {
            std::uint64_t sz = 0;
            std::memcpy(&sz, mmap_.data() + 8, sizeof(sz));
            if (sz > 0 && (16 + sz) <= mmap_.size()) {
                std::vector<std::uint8_t> buf(sz);
                std::memcpy(buf.data(), mmap_.data() + 16, static_cast<std::size_t>(sz));
                auto dec = serde::decode(buf);
                if (dec.is_err()) return Status::from_err(mem_error(0, "mmap decode"));
                data_ = std::move(dec.unwrap().entries);
            }
        }
        return Status::from_ok(std::monostate{});
    }

    Status put(std::string path, Record rec) {
        if (path.empty()) return Status::from_err(mem_error(0, "path"));
        data_[std::move(path)] = std::move(rec);
        return flush_if_needed_();
    }

    Result<std::optional<Record>> get(std::string_view path) const {
        auto it = data_.find(std::string(path));
        if (it == data_.end()) return Result<std::optional<Record>>::from_ok(std::nullopt);
        return Result<std::optional<Record>>::from_ok(it->second);
    }

    Status erase(std::string_view path) {
        data_.erase(std::string(path));
        return flush_if_needed_();
    }

    Result<std::vector<std::pair<std::string, Record>>> query(std::string_view text) const {
        zetha::query::text::Parser p;
        auto parsed = p.parse(text);
        if (parsed.is_err()) return Result<std::vector<std::pair<std::string, Record>>>::from_err(mem_error(0, "query parse"));
        const auto pq = parsed.unwrap();
        std::vector<std::pair<std::string, Record>> out;
        for (const auto& kv : data_) {
            if (match_path_(pq.path, kv.first) && match_predicates_(pq.path, kv.second)) {
                out.push_back(kv);
            }
        }
        return Result<std::vector<std::pair<std::string, Record>>>::from_ok(std::move(out));
    }

    Status dump_to_file(const std::string& path) const {
        Snapshot s;
        s.entries = data_;
        auto enc = serde::encode(s);
        if (enc.is_err()) return Status::from_err(mem_error(0, "serde encode"));
        std::ofstream of(path, std::ios::binary | std::ios::trunc);
        if (!of) return Status::from_err(mem_error(0, "dump open"));
        const auto& b = enc.unwrap();
        of.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
        if (!of) return Status::from_err(mem_error(0, "dump write"));
        return Status::from_ok(std::monostate{});
    }

    Status load_from_file(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return Status::from_err(mem_error(0, "load open"));
        std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto dec = serde::decode(b);
        if (dec.is_err()) return Status::from_err(mem_error(0, "serde decode"));
        data_ = std::move(dec.unwrap().entries);
        return flush_if_needed_();
    }

private:
    static std::vector<std::string> split_path_(std::string_view p) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : p) {
            if (c == '/') {
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    static bool eval_expr_(const zetha::query::Expr& ex, const Record& rec) {
        if (auto cmp = std::get_if<zetha::query::Compare>(&ex.v)) {
            const auto* lhs = std::get_if<zetha::query::AttributeRef>(&cmp->lhs->v);
            if (!lhs || cmp->op != zetha::query::CmpOp::Eq) return false;
            auto it = rec.fields.find(lhs->name);
            if (it == rec.fields.end()) return false;
            if (auto lit = std::get_if<zetha::query::Literal>(&cmp->rhs->v)) {
                if (std::holds_alternative<std::string>(lit->value)) return value_equals(it->second, Value{std::get<std::string>(lit->value)});
                if (std::holds_alternative<double>(lit->value)) return value_equals(it->second, Value{std::get<double>(lit->value)});
                if (std::holds_alternative<bool>(lit->value)) return value_equals(it->second, Value{std::get<bool>(lit->value)});
                return false;
            }
            if (auto nr = std::get_if<zetha::query::NameRef>(&cmp->rhs->v)) return value_equals(it->second, Value{nr->name});
        }
        return false;
    }

    static bool match_path_(const zetha::query::PathExpr& qpath, const std::string& key) {
        auto parts = split_path_(key);
        if (parts.size() != qpath.steps.size()) return false;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (qpath.steps[i].axis != zetha::query::Axis::Child) return false;
            if (qpath.steps[i].test.kind != zetha::query::NodeTestKind::Name) return false;
            if (qpath.steps[i].test.name != parts[i]) return false;
        }
        return true;
    }

    static bool match_predicates_(const zetha::query::PathExpr& p, const Record& rec) {
        for (const auto& s : p.steps) {
            for (const auto& pred : s.predicates) {
                if (!eval_expr_(pred.expr, rec)) return false;
            }
        }
        return true;
    }

    Status flush_if_needed_() {
        if (mode_ != Mode::MMapBacked) return Status::from_ok(std::monostate{});
        if (!mmap_.mapped()) return Status::from_err(mem_error(0, "mmap not open"));
        Snapshot s;
        s.entries = data_;
        auto enc = serde::encode(s);
        if (enc.is_err()) return Status::from_err(mem_error(0, "serde encode"));
        const auto& b = enc.unwrap();
        if (b.size() + 16 > mmap_.size()) return Status::from_err(mem_error(0, "mmap capacity"));
        std::memset(mmap_.data(), 0, mmap_.size());
        std::memcpy(mmap_.data(), "ZMEMv2\0", 7);
        const std::uint64_t sz = static_cast<std::uint64_t>(b.size());
        std::memcpy(mmap_.data() + 8, &sz, sizeof(sz));
        std::memcpy(mmap_.data() + 16, b.data(), b.size());
        return mmap_.sync();
    }

    Mode mode_ = Mode::PureRAM;
    std::string mmap_path_;
    std::map<std::string, Record> data_;
    MMapRegion mmap_;
};

} // namespace memv2

} // namespace zethamem
