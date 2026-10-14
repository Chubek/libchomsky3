#pragma once

#ifndef SERDETK_HPP
#define SERDETK_HPP

#include "DSLUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace serdetk {

inline constexpr std::string_view version = "0.2.0";

struct Value;
struct Document;
struct Object;
struct Array;
struct Binary;

struct SourceLocation;
struct Diagnostic;
struct Diagnostics;

class Error;
class FormatError;
class ParseError;
class ConversionError;
class QueryError;

class CompiledFormat;
class FormatRegistry;

namespace memory {
struct Manual {};
struct Managed {};
} // namespace memory

namespace formatting {
struct Options;
} // namespace formatting

namespace conversion {
enum class Policy;
struct Report;
} // namespace conversion

struct SourceLocation {
    std::string file {};
    std::size_t line {0};
    std::size_t column {0};
};

struct Diagnostic {
    enum class Severity { Note, Warning, Error };
    Severity severity {Severity::Error};
    std::string message {};
    std::optional<SourceLocation> location {};
};

struct Diagnostics {
    std::vector<Diagnostic> items {};
    void add(Diagnostic diag) { items.push_back(std::move(diag)); }
    bool has_errors() const {
        return std::any_of(items.begin(), items.end(), [](const Diagnostic& d) {
            return d.severity == Diagnostic::Severity::Error;
        });
    }
};

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};
class FormatError : public Error { public: using Error::Error; };
class ParseError : public Error { public: using Error::Error; };
class ConversionError : public Error { public: using Error::Error; };
class QueryError : public Error { public: using Error::Error; };

struct Binary {
    std::vector<std::uint8_t> bytes {};
    Binary() = default;
    explicit Binary(std::vector<std::uint8_t> b) : bytes(std::move(b)) {}
};

struct Object;
struct Array;

struct Value {
    using object_ptr = std::shared_ptr<Object>;
    using array_ptr = std::shared_ptr<Array>;
    using variant_type = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, Binary, object_ptr, array_ptr>;

    variant_type data {};

    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool v) : data(v) {}
    Value(int v) : data(static_cast<std::int64_t>(v)) {}
    Value(long v) : data(static_cast<std::int64_t>(v)) {}
    Value(long long v) : data(static_cast<std::int64_t>(v)) {}
    Value(unsigned v) : data(static_cast<std::uint64_t>(v)) {}
    Value(unsigned long v) : data(static_cast<std::uint64_t>(v)) {}
    Value(unsigned long long v) : data(static_cast<std::uint64_t>(v)) {}
    Value(double v) : data(v) {}
    Value(const char* v) : data(std::string(v)) {}
    Value(std::string v) : data(std::move(v)) {}
    Value(Binary v) : data(std::move(v)) {}
    Value(object_ptr v) : data(std::move(v)) {}
    Value(array_ptr v) : data(std::move(v)) {}

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_int() const { return std::holds_alternative<std::int64_t>(data); }
    bool is_uint() const { return std::holds_alternative<std::uint64_t>(data); }
    bool is_double() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_binary() const { return std::holds_alternative<Binary>(data); }
    bool is_object() const { return std::holds_alternative<object_ptr>(data); }
    bool is_array() const { return std::holds_alternative<array_ptr>(data); }

    Object& as_object();
    const Object& as_object() const;
    Array& as_array();
    const Array& as_array() const;

    std::string& as_string() { return std::get<std::string>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    Binary& as_binary() { return std::get<Binary>(data); }
    const Binary& as_binary() const { return std::get<Binary>(data); }
};

struct Object {
    std::map<std::string, Value> fields {};
    Value& operator[](std::string key) { return fields[std::move(key)]; }
    const Value& at(const std::string& key) const { return fields.at(key); }
    bool contains(const std::string& key) const { return fields.find(key) != fields.end(); }
    void set(std::string key, Value value) { fields[std::move(key)] = std::move(value); }
};

struct Array {
    std::vector<Value> items {};
    void push(Value value) { items.push_back(std::move(value)); }
    Value& operator[](std::size_t i) { return items[i]; }
    const Value& operator[](std::size_t i) const { return items[i]; }
};

inline Object& Value::as_object() { return *std::get<object_ptr>(data); }
inline const Object& Value::as_object() const { return *std::get<object_ptr>(data); }
inline Array& Value::as_array() { return *std::get<array_ptr>(data); }
inline const Array& Value::as_array() const { return *std::get<array_ptr>(data); }

struct Document {
    Value root {};
    std::optional<std::string> schema {};
    std::map<std::string, Value> metadata {};
    std::optional<std::string> source_format {};
};

namespace formatting {
struct Options {
    std::string indent = "  ";
    std::string newline = "\n";
    bool pretty = true;
    bool space_after_colon = true;
    bool space_after_comma = true;
    bool sort_keys = false;
    bool trailing_newline = true;
};
} // namespace formatting

namespace conversion {
enum class Policy { Strict, LossAware, BestEffort };
enum class LossKind { None, Ordering, TypeNarrowing, TagLoss, AttributeLoss, BinaryCarrier, Placeholder };
struct Report {
    std::vector<std::string> warnings {};
    std::vector<std::string> errors {};
    std::vector<LossKind> losses {};
    bool ok() const { return errors.empty(); }
};
} // namespace conversion

enum class FormatCategory { Textual, Binary };

class CompiledFormat {
public:
    using LoadStringFn = std::function<Document(std::string_view)>;
    using LoadBytesFn = std::function<Document(const std::vector<std::uint8_t>&)>;
    using DumpStringFn = std::function<std::string(const Document&, const formatting::Options&)>;
    using DumpBytesFn = std::function<std::vector<std::uint8_t>(const Document&)>;

    std::string name {};
    FormatCategory category {FormatCategory::Textual};
    std::vector<std::string> extensions {};
    std::vector<std::string> mime_types {};
    std::string encoding {};
    std::vector<std::string> query_engines {};
    std::vector<std::string> validation_standards {};
    std::vector<std::string> conversion_adapters {};
    std::unordered_map<std::string, std::vector<std::string>> sktl_sections {};
    formatting::Options formatting_defaults {};

    LoadStringFn load_string_fn {};
    LoadBytesFn load_bytes_fn {};
    DumpStringFn dump_string_fn {};
    DumpBytesFn dump_bytes_fn {};

    CompiledFormat() = default;
    explicit CompiledFormat(std::string n) : name(std::move(n)) {}

    bool is_textual() const noexcept { return category == FormatCategory::Textual; }
    bool is_binary() const noexcept { return category == FormatCategory::Binary; }

    Document load_file(const std::filesystem::path& path) const;
    Document load_string(std::string_view input) const;
    Document load_bytes(const std::vector<std::uint8_t>& bytes) const;

    std::string dump_string(const Document& doc, const formatting::Options& opt = {}) const;
    std::vector<std::uint8_t> dump_bytes(const Document& doc) const;
    void dump_file(const Document& doc, const std::filesystem::path& path, const formatting::Options& opt = {}) const;
};

class FormatRegistry {
public:
    static FormatRegistry& instance() {
        static FormatRegistry registry;
        return registry;
    }
    void register_format(CompiledFormat format) { formats_[format.name] = std::move(format); }
    CompiledFormat* find(const std::string& name) {
        auto it = formats_.find(name);
        return it == formats_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, CompiledFormat> formats_ {};
};

namespace sktl {
struct Descriptor {
    std::string name {};
    FormatCategory category {FormatCategory::Textual};
    std::vector<std::string> extensions {};
    std::unordered_map<std::string, std::vector<std::string>> sections {};
};

inline std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

inline Descriptor parse(std::string_view text) {
    Descriptor d;
    std::istringstream in{std::string(text)};
    std::string line;
    std::string section;
    int depth = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        if (line == "}") {
            if (depth == 0) throw ParseError("Invalid SKTL: unmatched closing brace");
            --depth;
            if (depth == 0) section.clear();
            continue;
        }
        if (line.back() == '{') {
            ++depth;
            if (depth == 1) section = trim(line.substr(0, line.size() - 1));
            continue;
        }

        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (depth == 0) {
            if (key == "format") {
                ls >> d.name;
            } else if (key == "category") {
                std::string cat;
                ls >> cat;
                d.category = (cat == "binary") ? FormatCategory::Binary : FormatCategory::Textual;
            } else if (key == "extensions") {
                for (std::string ext; ls >> ext; ) d.extensions.push_back(ext);
            } else if (key == "mime") {
                std::string mime;
                while (ls >> mime) d.sections["__top_mime__"].push_back(mime);
            } else if (key == "encoding") {
                std::string enc;
                ls >> enc;
                if (!enc.empty()) d.sections["__top_encoding__"].push_back(enc);
            }
        } else {
            d.sections[section].push_back(line);
        }
    }
    if (depth != 0) throw ParseError("Invalid SKTL: unclosed section");
    if (d.name.empty()) throw ParseError("Invalid SKTL: missing format name");
    return d;
}

inline CompiledFormat compile(const Descriptor& d);
inline CompiledFormat compile_string(std::string_view text) { return compile(parse(text)); }
inline CompiledFormat compile_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error("Failed to open SKTL file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return compile_string(ss.str());
}

inline void register_directory(const std::filesystem::path& dir, FormatRegistry& reg = FormatRegistry::instance()) {
    if (!std::filesystem::exists(dir)) return;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".sktl") continue;
        reg.register_format(compile_file(e.path()));
    }
}
} // namespace sktl

namespace native {
struct FormatDSL : dsl::DSL<FormatDSL, dsl::Pipeline, dsl::PatternMatch, dsl::AST, dsl::Rewrite> {
    struct Definition { CompiledFormat format {}; };
    static Definition textual(std::string name) {
        Definition d; d.format.name = std::move(name); d.format.category = FormatCategory::Textual; return d;
    }
    static Definition binary(std::string name) {
        Definition d; d.format.name = std::move(name); d.format.category = FormatCategory::Binary; return d;
    }
    static Definition extension(Definition d, std::string ext) { d.format.extensions.push_back(std::move(ext)); return d; }
    static CompiledFormat build(Definition d) { return std::move(d.format); }
};
} // namespace native

namespace query {
struct Result { std::vector<const Value*> values {}; };
class API {
public:
    explicit API(const Document& doc) : current_(&doc.root) {}
    API& field(const std::string& name) {
        if (!current_ || !current_->is_object()) throw QueryError("Current node is not an object");
        current_ = &current_->as_object().at(name);
        return *this;
    }
    API& index(std::size_t i) {
        if (!current_ || !current_->is_array()) throw QueryError("Current node is not an array");
        current_ = &current_->as_array()[i];
        return *this;
    }
template <typename T> T get() const;
    const Value& value() const { if (!current_) throw QueryError("No current query value"); return *current_; }

private:
    const Value* current_ = nullptr;
};
template <> inline std::string API::get<std::string>() const { return value().as_string(); }
template <> inline bool API::get<bool>() const {
    if (!value().is_bool()) throw QueryError("Query value is not bool");
    return std::get<bool>(value().data);
}
template <> inline std::int64_t API::get<std::int64_t>() const {
    if (!value().is_int()) throw QueryError("Query value is not int64");
    return std::get<std::int64_t>(value().data);
}
template <> inline std::uint64_t API::get<std::uint64_t>() const {
    if (!value().is_uint()) throw QueryError("Query value is not uint64");
    return std::get<std::uint64_t>(value().data);
}
template <> inline double API::get<double>() const {
    if (value().is_double()) return std::get<double>(value().data);
    if (value().is_int()) return static_cast<double>(std::get<std::int64_t>(value().data));
    if (value().is_uint()) return static_cast<double>(std::get<std::uint64_t>(value().data));
    throw QueryError("Query value is not numeric");
}
template <> inline const Object& API::get<const Object&>() const {
    if (!value().is_object()) throw QueryError("Query value is not object");
    return value().as_object();
}
template <> inline const Array& API::get<const Array&>() const {
    if (!value().is_array()) throw QueryError("Query value is not array");
    return value().as_array();
}

class STKQ {
public:
    static Result run(const Document& doc, std::string_view expr) {
        Result out;
        const Value* cur = &doc.root;
        std::string path(expr);
        if (path.rfind("root", 0) == 0) path.erase(0, 4);
        std::size_t i = 0;
        while (i < path.size()) {
            if (path[i] == '.') { ++i; continue; }
            if (!cur) break;
            if (path[i] == '[') {
                const auto e = path.find(']', i);
                if (e == std::string::npos || !cur->is_array()) return {};
                const auto token = path.substr(i + 1, e - i - 1);
                if (token == "*") {
                    for (const auto& item : cur->as_array().items) out.values.push_back(&item);
                    return out;
                }
                const std::size_t idx = static_cast<std::size_t>(std::stoull(token));
                if (idx >= cur->as_array().items.size()) return {};
                cur = &cur->as_array().items[idx];
                i = e + 1;
                continue;
            }
            std::size_t j = i;
            while (j < path.size() && path[j] != '.' && path[j] != '[') ++j;
            const std::string key = path.substr(i, j - i);
            if (!cur->is_object() || !cur->as_object().contains(key)) return {};
            cur = &cur->as_object().at(key);
            i = j;
        }
        if (cur) out.values.push_back(cur);
        return out;
    }
};
class JQ { public: static Result run(const Document&, std::string_view) { return {}; } };
class SPARQL { public: static Result run(const Document&, std::string_view) { return {}; } };
} // namespace query

struct ValidationReport {
    bool success = true;
    std::vector<std::string> errors {};
    std::vector<std::string> warnings {};
    std::vector<Diagnostic> diagnostics {};
    void add_error(std::string msg, std::string path = {}) {
        success = false;
        errors.push_back(msg);
        Diagnostic d;
        d.severity = Diagnostic::Severity::Error;
        d.message = std::move(msg);
        if (!path.empty()) {
            SourceLocation loc;
            loc.file = std::move(path);
            d.location = std::move(loc);
        }
        diagnostics.push_back(std::move(d));
    }
    void add_warning(std::string msg, std::string path = {}) {
        warnings.push_back(msg);
        Diagnostic d;
        d.severity = Diagnostic::Severity::Warning;
        d.message = std::move(msg);
        if (!path.empty()) {
            SourceLocation loc;
            loc.file = std::move(path);
            d.location = std::move(loc);
        }
        diagnostics.push_back(std::move(d));
    }
    void print(std::ostream& os) const {
        os << (success ? "valid" : "invalid") << "\n";
        for (const auto& w : warnings) os << "warning: " << w << "\n";
        for (const auto& e : errors) os << "error: " << e << "\n";
    }
};

class SectionBuilder;
class ObjectBuilder;
class ArrayBuilder;

class SectionBuilder {
public:
    explicit SectionBuilder(Value* target) : target_(target) {}
    template <typename T> SectionBuilder& set(const std::string& key, T&& value) {
        ensure_object();
        target_->as_object().set(key, Value(std::forward<T>(value)));
        return *this;
    }
    ObjectBuilder add_object(const std::string& key);
    ArrayBuilder add_list(const std::string& key);

private:
    void ensure_object() { if (!target_->is_object()) target_->data = std::make_shared<Object>(); }
    Value* target_ = nullptr;
};

class ObjectBuilder {
public:
    explicit ObjectBuilder(Value* target) : target_(target) {}
    template <typename T> ObjectBuilder& set(const std::string& key, T&& value) {
        ensure_object();
        target_->as_object().set(key, Value(std::forward<T>(value)));
        return *this;
    }
    ObjectBuilder add_object(const std::string& key) {
        ensure_object();
        auto ptr = std::make_shared<Object>();
        target_->as_object().set(key, Value(ptr));
        return ObjectBuilder(&target_->as_object().fields.at(key));
    }
    ArrayBuilder add_list(const std::string& key);

private:
    void ensure_object() { if (!target_->is_object()) target_->data = std::make_shared<Object>(); }
    Value* target_ = nullptr;
};

class ArrayBuilder {
public:
    explicit ArrayBuilder(Value* target) : target_(target) {}
    template <typename T> ArrayBuilder& push(T&& value) {
        ensure_array();
        target_->as_array().push(Value(std::forward<T>(value)));
        return *this;
    }

private:
    void ensure_array() { if (!target_->is_array()) target_->data = std::make_shared<Array>(); }
    Value* target_ = nullptr;
};

inline ObjectBuilder SectionBuilder::add_object(const std::string& key) {
    ensure_object();
    auto ptr = std::make_shared<Object>();
    target_->as_object().set(key, Value(ptr));
    return ObjectBuilder(&target_->as_object().fields.at(key));
}
inline ArrayBuilder SectionBuilder::add_list(const std::string& key) {
    ensure_object();
    auto ptr = std::make_shared<Array>();
    target_->as_object().set(key, Value(ptr));
    return ArrayBuilder(&target_->as_object().fields.at(key));
}
inline ArrayBuilder ObjectBuilder::add_list(const std::string& key) {
    ensure_object();
    auto ptr = std::make_shared<Array>();
    target_->as_object().set(key, Value(ptr));
    return ArrayBuilder(&target_->as_object().fields.at(key));
}

template <typename MemoryPolicy = memory::Manual>
class Creator {
public:
    explicit Creator(const CompiledFormat* fmt) : format_(fmt) { document_.root.data = std::make_shared<Object>(); document_.source_format = fmt ? fmt->name : std::optional<std::string>{}; }
    void set_schema(std::string schema) { document_.schema = std::move(schema); }
    SectionBuilder add_section(const std::string& name) {
        auto ptr = std::make_shared<Object>();
        document_.root.as_object().set(name, Value(ptr));
        return SectionBuilder(&document_.root.as_object().fields.at(name));
    }
    Document& document() { return document_; }
    std::string str(const formatting::Options& opt = {}) const { return format_->dump_string(document_, opt); }
    void dump(const std::filesystem::path& path, const formatting::Options& opt = {}) const { format_->dump_file(document_, path, opt); }

private:
    Document document_ {};
    const CompiledFormat* format_ = nullptr;
};

class SimplePrinter {
public:
    explicit SimplePrinter(const CompiledFormat* fmt) : format_(fmt) {}
    std::string print(const Document& doc) const {
        formatting::Options opt; opt.pretty = false; opt.space_after_colon = false; opt.space_after_comma = false; opt.trailing_newline = false;
        return format_->dump_string(doc, opt);
    }
private:
    const CompiledFormat* format_;
};
class PrettyPrinter {
public:
    explicit PrettyPrinter(const CompiledFormat* fmt, formatting::Options opt = {}) : format_(fmt), options_(std::move(opt)) {}
    std::string print(const Document& doc) const { return format_->dump_string(doc, options_); }
private:
    const CompiledFormat* format_;
    formatting::Options options_ {};
};

struct Minify {
    static std::string run(const CompiledFormat& format, const Document& doc) {
        formatting::Options opt; opt.pretty = false; opt.space_after_colon = false; opt.space_after_comma = false; opt.trailing_newline = false;
        return format.dump_string(doc, opt);
    }
};

class Editor {
public:
    explicit Editor(Document doc, const CompiledFormat* format) : document_(std::move(doc)), format_(format) {}
    static Editor open(const CompiledFormat& format, const std::filesystem::path& path) { return Editor(format.load_file(path), &format); }
    Document& document() { return document_; }
    void dump(const std::filesystem::path& path, const formatting::Options& opt = {}) const { format_->dump_file(document_, path, opt); }
private:
    Document document_ {};
    const CompiledFormat* format_;
};

class Validator {
public:
    explicit Validator(Document schema_doc) : schema_(std::move(schema_doc)) {}
    static std::shared_ptr<Validator> from_schema(std::string_view schema_text, const CompiledFormat& fmt) {
        return std::make_shared<Validator>(fmt.load_string(schema_text));
    }
    static std::shared_ptr<Validator> from_schema_file(const std::filesystem::path& p, const CompiledFormat& fmt) {
        return std::make_shared<Validator>(fmt.load_file(p));
    }
    ValidationReport validate(const Document& doc) const {
        ValidationReport r;
        if (!schema_.root.is_object()) {
            r.success = false;
            r.errors.push_back("Schema root must be object");
            return r;
        }
        validate_node(schema_.root.as_object(), doc.root, "$", r);
        return r;
    }
    bool validate(const Document& doc, std::ostream& os) const {
        const auto rep = validate(doc);
        rep.print(os);
        return rep.success;
    }
private:
    static bool value_equals(const Value& a, const Value& b) {
        if (a.data.index() != b.data.index()) return false;
        if (a.is_null()) return true;
        if (a.is_bool()) return std::get<bool>(a.data) == std::get<bool>(b.data);
        if (a.is_int()) return std::get<std::int64_t>(a.data) == std::get<std::int64_t>(b.data);
        if (a.is_uint()) return std::get<std::uint64_t>(a.data) == std::get<std::uint64_t>(b.data);
        if (a.is_double()) return std::get<double>(a.data) == std::get<double>(b.data);
        if (a.is_string()) return a.as_string() == b.as_string();
        if (a.is_binary()) return a.as_binary().bytes == b.as_binary().bytes;
        if (a.is_array()) {
            const auto& x = a.as_array().items;
            const auto& y = b.as_array().items;
            if (x.size() != y.size()) return false;
            for (std::size_t i = 0; i < x.size(); ++i) if (!value_equals(x[i], y[i])) return false;
            return true;
        }
        const auto& xo = a.as_object().fields;
        const auto& yo = b.as_object().fields;
        if (xo.size() != yo.size()) return false;
        for (const auto& kv : xo) {
            if (!yo.contains(kv.first)) return false;
            if (!value_equals(kv.second, yo.at(kv.first))) return false;
        }
        return true;
    }
    static bool is_type(const Value& v, const std::string& t) {
        if (t == "null") return v.is_null();
        if (t == "boolean") return v.is_bool();
        if (t == "integer") return v.is_int() || v.is_uint();
        if (t == "number") return v.is_int() || v.is_uint() || v.is_double();
        if (t == "string") return v.is_string();
        if (t == "object") return v.is_object();
        if (t == "array") return v.is_array();
        return false;
    }
    static void fail(ValidationReport& r, const std::string& msg, const std::string& path = {}) { r.add_error(msg, path); }
    static void validate_node(const Object& schema_obj, const Value& node, const std::string& path, ValidationReport& r) {
        if (schema_obj.contains("type")) {
            const auto& t = schema_obj.at("type");
            bool ok = false;
            if (t.is_string()) ok = is_type(node, t.as_string());
            else if (t.is_array()) {
                for (const auto& it : t.as_array().items) {
                    if (it.is_string() && is_type(node, it.as_string())) { ok = true; break; }
                }
            } else fail(r, path + ": schema.type must be string or string[]");
            if (!ok) fail(r, path + ": type mismatch");
        }
        if (schema_obj.contains("enum")) {
            const auto& e = schema_obj.at("enum");
            bool found = false;
            if (e.is_array()) {
                for (const auto& v : e.as_array().items) {
                    if (value_equals(v, node)) { found = true; break; }
                }
            }
            if (!found) fail(r, path + ": enum mismatch");
        }
        if (schema_obj.contains("const")) {
            if (!value_equals(schema_obj.at("const"), node)) fail(r, path + ": const mismatch", path);
        }
        if (node.is_string()) {
            if (schema_obj.contains("minLength") && (schema_obj.at("minLength").is_int() || schema_obj.at("minLength").is_uint())) {
                const std::size_t n = node.as_string().size();
                const std::size_t m = schema_obj.at("minLength").is_int() ? static_cast<std::size_t>(std::get<std::int64_t>(schema_obj.at("minLength").data)) : static_cast<std::size_t>(std::get<std::uint64_t>(schema_obj.at("minLength").data));
                if (n < m) fail(r, path + ": minLength violated", path);
            }
            if (schema_obj.contains("maxLength") && (schema_obj.at("maxLength").is_int() || schema_obj.at("maxLength").is_uint())) {
                const std::size_t n = node.as_string().size();
                const std::size_t m = schema_obj.at("maxLength").is_int() ? static_cast<std::size_t>(std::get<std::int64_t>(schema_obj.at("maxLength").data)) : static_cast<std::size_t>(std::get<std::uint64_t>(schema_obj.at("maxLength").data));
                if (n > m) fail(r, path + ": maxLength violated", path);
            }
            if (schema_obj.contains("pattern") && schema_obj.at("pattern").is_string()) {
                std::regex re(schema_obj.at("pattern").as_string());
                if (!std::regex_search(node.as_string(), re)) fail(r, path + ": pattern mismatch", path);
            }
        }
        if (schema_obj.contains("required") && node.is_object()) {
            const auto& req = schema_obj.at("required");
            if (req.is_array()) {
                for (const auto& name : req.as_array().items) {
                    if (!name.is_string()) { fail(r, path + ": required entry must be string"); continue; }
                    if (!node.as_object().contains(name.as_string())) fail(r, path + "." + name.as_string() + ": missing required property");
                }
            }
        }
        if (schema_obj.contains("properties") && node.is_object()) {
            const auto& props = schema_obj.at("properties");
            if (props.is_object()) {
                for (const auto& kv : props.as_object().fields) {
                    if (!kv.second.is_object()) continue;
                    if (!node.as_object().contains(kv.first)) continue;
                    validate_node(kv.second.as_object(), node.as_object().at(kv.first), path + "." + kv.first, r);
                }
                if (schema_obj.contains("additionalProperties")) {
                    const auto& ap = schema_obj.at("additionalProperties");
                    if (ap.is_bool() && !std::get<bool>(ap.data)) {
                        for (const auto& field : node.as_object().fields) {
                            if (!props.as_object().contains(field.first)) fail(r, path + "." + field.first + ": additional property not allowed");
                        }
                    }
                }
            }
        }
        if (schema_obj.contains("items") && node.is_array()) {
            const auto& items = schema_obj.at("items");
            if (items.is_object()) {
                for (std::size_t i = 0; i < node.as_array().items.size(); ++i) {
                    validate_node(items.as_object(), node.as_array().items[i], path + "[" + std::to_string(i) + "]", r);
                }
            } else if (items.is_array()) {
                const std::size_t n = std::min(items.as_array().items.size(), node.as_array().items.size());
                for (std::size_t i = 0; i < n; ++i) {
                    if (items.as_array().items[i].is_object()) validate_node(items.as_array().items[i].as_object(), node.as_array().items[i], path + "[" + std::to_string(i) + "]", r);
                }
            }
        }
        if (node.is_array()) {
            if (schema_obj.contains("minItems") && (schema_obj.at("minItems").is_int() || schema_obj.at("minItems").is_uint())) {
                const std::size_t min_items = schema_obj.at("minItems").is_int()
                    ? static_cast<std::size_t>(std::get<std::int64_t>(schema_obj.at("minItems").data))
                    : static_cast<std::size_t>(std::get<std::uint64_t>(schema_obj.at("minItems").data));
                if (node.as_array().items.size() < min_items) fail(r, path + ": minItems violated");
            }
            if (schema_obj.contains("maxItems") && (schema_obj.at("maxItems").is_int() || schema_obj.at("maxItems").is_uint())) {
                const std::size_t max_items = schema_obj.at("maxItems").is_int()
                    ? static_cast<std::size_t>(std::get<std::int64_t>(schema_obj.at("maxItems").data))
                    : static_cast<std::size_t>(std::get<std::uint64_t>(schema_obj.at("maxItems").data));
                if (node.as_array().items.size() > max_items) fail(r, path + ": maxItems violated");
            }
        }
        if (node.is_int() || node.is_uint() || node.is_double()) {
            const double v = node.is_double() ? std::get<double>(node.data) : (node.is_int() ? static_cast<double>(std::get<std::int64_t>(node.data)) : static_cast<double>(std::get<std::uint64_t>(node.data)));
            if (schema_obj.contains("minimum") && (schema_obj.at("minimum").is_int() || schema_obj.at("minimum").is_uint() || schema_obj.at("minimum").is_double())) {
                const double m = schema_obj.at("minimum").is_double() ? std::get<double>(schema_obj.at("minimum").data) : (schema_obj.at("minimum").is_int() ? static_cast<double>(std::get<std::int64_t>(schema_obj.at("minimum").data)) : static_cast<double>(std::get<std::uint64_t>(schema_obj.at("minimum").data)));
                if (v < m) fail(r, path + ": minimum violated");
            }
            if (schema_obj.contains("maximum") && (schema_obj.at("maximum").is_int() || schema_obj.at("maximum").is_uint() || schema_obj.at("maximum").is_double())) {
                const double m = schema_obj.at("maximum").is_double() ? std::get<double>(schema_obj.at("maximum").data) : (schema_obj.at("maximum").is_int() ? static_cast<double>(std::get<std::int64_t>(schema_obj.at("maximum").data)) : static_cast<double>(std::get<std::uint64_t>(schema_obj.at("maximum").data)));
                if (v > m) fail(r, path + ": maximum violated");
            }
            if (schema_obj.contains("uniqueItems") && schema_obj.at("uniqueItems").is_bool() && std::get<bool>(schema_obj.at("uniqueItems").data)) {
                for (std::size_t i = 0; i < node.as_array().items.size(); ++i) {
                    for (std::size_t j = i + 1; j < node.as_array().items.size(); ++j) {
                        if (value_equals(node.as_array().items[i], node.as_array().items[j])) fail(r, path + ": uniqueItems violated", path);
                    }
                }
            }
        }
        if (schema_obj.contains("allOf") && schema_obj.at("allOf").is_array()) {
            for (const auto& s : schema_obj.at("allOf").as_array().items) if (s.is_object()) validate_node(s.as_object(), node, path, r);
        }
    }
    Document schema_ {};
};

inline std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error("Failed to open file for reading: " + path.string());
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
inline std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error("Failed to open file for reading: " + path.string());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
inline void write_text_file(const std::filesystem::path& path, std::string_view text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw Error("Failed to open file for writing: " + path.string());
    out << text;
}
inline void write_binary_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw Error("Failed to open file for writing: " + path.string());
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

namespace detail {
inline std::string trim_copy(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

inline Value parse_scalar_token(std::string token) {
    token = trim_copy(token);
    if (token == "null" || token == "~") return Value(nullptr);
    if (token == "true") return Value(true);
    if (token == "false") return Value(false);
    if (!token.empty() && token.front() == '"' && token.back() == '"' && token.size() >= 2) return Value(token.substr(1, token.size() - 2));
    try { std::size_t idx = 0; long long v = std::stoll(token, &idx); if (idx == token.size()) return Value(v); } catch (...) {}
    try { std::size_t idx = 0; double v = std::stod(token, &idx); if (idx == token.size()) return Value(v); } catch (...) {}
    return Value(token);
}

inline std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream in{std::string(text)};
    for (std::string line; std::getline(in, line); ) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

inline std::size_t leading_spaces(std::string_view s) {
    std::size_t n = 0;
    while (n < s.size() && s[n] == ' ') ++n;
    return n;
}
class JsonParser {
public:
    explicit JsonParser(std::string_view in) : in_(in) {}
    Value parse_value() {
        skip_ws();
        if (eof()) throw ParseError("Unexpected end of JSON");
        const char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return Value(parse_string());
        if (c == 't') return parse_literal("true", Value(true));
        if (c == 'f') return parse_literal("false", Value(false));
        if (c == 'n') return parse_literal("null", Value(nullptr));
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        throw ParseError("Invalid JSON token");
    }
    void expect_end() { skip_ws(); if (!eof()) throw ParseError("Trailing JSON content"); }

private:
    Value parse_object() {
        consume('{');
        auto obj = std::make_shared<Object>();
        skip_ws();
        if (consume_if('}')) return Value(obj);
        while (true) {
            skip_ws();
            if (peek() != '"') throw ParseError("Expected object key string");
            std::string key = parse_string();
            skip_ws();
            consume(':');
            obj->set(std::move(key), parse_value());
            skip_ws();
            if (consume_if('}')) break;
            consume(',');
        }
        return Value(obj);
    }
    Value parse_array() {
        consume('[');
        auto arr = std::make_shared<Array>();
        skip_ws();
        if (consume_if(']')) return Value(arr);
        while (true) {
            arr->push(parse_value());
            skip_ws();
            if (consume_if(']')) break;
            consume(',');
        }
        return Value(arr);
    }
    Value parse_number() {
        const std::size_t start = pos_;
        if (consume_if('-')) {}
        if (!eof() && peek() == '0') { ++pos_; } else {
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) throw ParseError("Invalid number");
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }
        bool is_float = false;
        if (!eof() && peek() == '.') {
            is_float = true;
            ++pos_;
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) throw ParseError("Invalid number fraction");
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }
        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            is_float = true;
            ++pos_;
            if (!eof() && (peek() == '+' || peek() == '-')) ++pos_;
            if (eof() || !std::isdigit(static_cast<unsigned char>(peek()))) throw ParseError("Invalid exponent");
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
        }
        const std::string token(in_.substr(start, pos_ - start));
        if (is_float) return Value(std::stod(token));
        if (!token.empty() && token[0] == '-') return Value(static_cast<long long>(std::stoll(token)));
        return Value(static_cast<unsigned long long>(std::stoull(token)));
    }
    std::string parse_string() {
        consume('"');
        std::string out;
        const auto hexv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };
        const auto append_utf8 = [&](unsigned code) {
            if (code <= 0x7F) out.push_back(static_cast<char>(code));
            else if (code <= 0x7FF) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else if (code <= 0xFFFF) {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        };
        while (!eof()) {
            const char c = next();
            if (c == '"') return out;
            if (c == '\\') {
                if (eof()) throw ParseError("Invalid escape");
                const char e = next();
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > in_.size()) throw ParseError("Invalid unicode escape");
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            int h = hexv(in_[pos_++]);
                            if (h < 0) throw ParseError("Invalid unicode escape");
                            code = (code << 4U) | static_cast<unsigned>(h);
                        }
                        append_utf8(code);
                        break;
                    }
                    default: throw ParseError("Unsupported JSON escape");
                }
            } else {
                out.push_back(c);
            }
        }
        throw ParseError("Unterminated string");
    }
    Value parse_literal(std::string_view literal, Value value) {
        for (char c : literal) {
            if (eof() || next() != c) throw ParseError("Invalid JSON literal");
        }
        return value;
    }
    bool eof() const { return pos_ >= in_.size(); }
    char peek() const { return in_[pos_]; }
    char next() { return in_[pos_++]; }
    void consume(char expected) { if (eof() || next() != expected) throw ParseError("Unexpected JSON character"); }
    bool consume_if(char c) { if (!eof() && peek() == c) { ++pos_; return true; } return false; }
    void skip_ws() { while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) ++pos_; }

    std::string_view in_;
    std::size_t pos_ = 0;
};

inline void emit_json_string(std::ostringstream& out, const std::string& s) {
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    out << '"';
}

inline void emit_json_value(std::ostringstream& out, const Value& v, const formatting::Options& opt, std::size_t depth) {
    if (v.is_null()) { out << "null"; return; }
    if (v.is_bool()) { out << (std::get<bool>(v.data) ? "true" : "false"); return; }
    if (v.is_int()) { out << std::get<std::int64_t>(v.data); return; }
    if (v.is_uint()) { out << std::get<std::uint64_t>(v.data); return; }
    if (v.is_double()) { out << std::get<double>(v.data); return; }
    if (v.is_string()) { emit_json_string(out, std::get<std::string>(v.data)); return; }
    if (v.is_binary()) throw FormatError("JSON cannot emit binary values");
    const auto indent = [&](std::size_t d) { for (std::size_t i = 0; i < d; ++i) out << opt.indent; };
    if (v.is_array()) {
        const auto& arr = v.as_array().items;
        out << '[';
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i != 0) out << ',' << (opt.pretty && opt.space_after_comma ? " " : "");
            if (opt.pretty) { out << opt.newline; indent(depth + 1); }
            emit_json_value(out, arr[i], opt, depth + 1);
        }
        if (opt.pretty && !arr.empty()) { out << opt.newline; indent(depth); }
        out << ']';
        return;
    }
    const auto& obj = v.as_object().fields;
    out << '{';
    bool first = true;
    for (const auto& kv : obj) {
        if (!first) out << ',' << (opt.pretty && opt.space_after_comma ? " " : "");
        if (opt.pretty) { out << opt.newline; indent(depth + 1); }
        emit_json_string(out, kv.first);
        out << ':';
        if (opt.space_after_colon) out << ' ';
        emit_json_value(out, kv.second, opt, depth + 1);
        first = false;
    }
    if (opt.pretty && !obj.empty()) { out << opt.newline; indent(depth); }
    out << '}';
}

inline Document parse_yaml_basic(std::string_view text) {
    struct Parser {
        std::vector<std::string> lines;
        std::size_t i = 0;

        bool done() const { return i >= lines.size(); }
        std::string_view current() const { return lines[i]; }

        void skip_ignorable() {
            while (!done()) {
                std::string t = trim_copy(current());
                if (t.empty() || t.rfind("#", 0) == 0) { ++i; continue; }
                break;
            }
        }

        Value parse_block(std::size_t indent) {
            skip_ignorable();
            if (done()) return Value(nullptr);
            const std::string t = trim_copy(current());
            if (t.rfind("- ", 0) == 0) return parse_sequence(indent);
            return parse_mapping(indent);
        }

        Value parse_sequence(std::size_t indent) {
            auto arr = std::make_shared<Array>();
            while (!done()) {
                skip_ignorable();
                if (done()) break;
                const std::string_view line = current();
                const std::size_t cur_indent = leading_spaces(line);
                if (cur_indent < indent) break;
                if (cur_indent != indent) throw ParseError("Invalid YAML indentation in sequence");
                std::string item = trim_copy(line.substr(cur_indent));
                if (item.rfind("- ", 0) != 0) break;
                std::string rhs = trim_copy(item.substr(2));
                ++i;
                if (rhs.empty()) arr->push(parse_block(indent + 2));
                else arr->push(parse_scalar_token(rhs));
            }
            return Value(arr);
        }

        Value parse_mapping(std::size_t indent) {
            auto obj = std::make_shared<Object>();
            while (!done()) {
                skip_ignorable();
                if (done()) break;
                const std::string_view line = current();
                const std::size_t cur_indent = leading_spaces(line);
                if (cur_indent < indent) break;
                if (cur_indent != indent) throw ParseError("Invalid YAML indentation in mapping");
                const std::string body = trim_copy(line.substr(cur_indent));
                if (body.rfind("- ", 0) == 0) break;
                const auto colon = body.find(':');
                if (colon == std::string::npos) throw ParseError("Invalid YAML mapping entry");
                std::string key = trim_copy(body.substr(0, colon));
                std::string rhs = trim_copy(body.substr(colon + 1));
                ++i;
                if (rhs.empty()) {
                    skip_ignorable();
                    if (done()) { obj->set(std::move(key), Value(nullptr)); continue; }
                    const std::size_t next_indent = leading_spaces(current());
                    if (trim_copy(current()).rfind("- ", 0) == 0 && next_indent == indent) obj->set(std::move(key), parse_sequence(indent));
                    else obj->set(std::move(key), parse_block(indent + 2));
                } else obj->set(std::move(key), parse_scalar_token(rhs));
            }
            return Value(obj);
        }
    };

    Parser p{split_lines(text)};
    Document d;
    d.root = p.parse_block(0);
    return d;
}

inline void emit_yaml_value(std::ostringstream& out, const Value& v, std::size_t depth) {
    const auto indent = [&](std::size_t d) { for (std::size_t i = 0; i < d; ++i) out << "  "; };
    if (v.is_null()) { out << "null"; return; }
    if (v.is_bool()) { out << (std::get<bool>(v.data) ? "true" : "false"); return; }
    if (v.is_int()) { out << std::get<std::int64_t>(v.data); return; }
    if (v.is_uint()) { out << std::get<std::uint64_t>(v.data); return; }
    if (v.is_double()) { out << std::get<double>(v.data); return; }
    if (v.is_string()) { out << v.as_string(); return; }
    if (v.is_binary()) { out << "<binary:" << v.as_binary().bytes.size() << ">"; return; }
    if (v.is_array()) {
        for (const auto& item : v.as_array().items) {
            indent(depth);
            out << "- ";
            if (item.is_object() || item.is_array()) { out << "\n"; emit_yaml_value(out, item, depth + 1); }
            else { emit_yaml_value(out, item, depth); out << "\n"; }
        }
        return;
    }
    for (const auto& kv : v.as_object().fields) {
        indent(depth);
        out << kv.first << ":";
        if (kv.second.is_object() || kv.second.is_array()) {
            out << "\n";
            emit_yaml_value(out, kv.second, depth + 1);
        } else {
            out << " ";
            emit_yaml_value(out, kv.second, depth);
            out << "\n";
        }
    }
}

inline Document parse_xml_basic(std::string_view text) {
    struct Parser {
        std::string_view s;
        std::size_t pos = 0;

        bool eof() const { return pos >= s.size(); }
        char peek() const { return s[pos]; }
        char next() { return s[pos++]; }

        void skip_ws() { while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) ++pos; }
        bool starts_with(std::string_view t) const { return s.substr(pos, t.size()) == t; }
        void consume(char c) { if (eof() || next() != c) throw ParseError("Invalid XML syntax"); }

        void skip_misc() {
            while (!eof()) {
                skip_ws();
                if (starts_with("<?")) {
                    pos += 2;
                    const auto e = s.find("?>", pos);
                    if (e == std::string_view::npos) throw ParseError("Unterminated XML declaration");
                    pos = e + 2;
                    continue;
                }
                if (starts_with("<!--")) {
                    pos += 4;
                    const auto e = s.find("-->", pos);
                    if (e == std::string_view::npos) throw ParseError("Unterminated XML comment");
                    pos = e + 3;
                    continue;
                }
                if (starts_with("<!DOCTYPE")) {
                    const auto e = s.find('>', pos);
                    if (e == std::string_view::npos) throw ParseError("Unterminated XML doctype");
                    pos = e + 1;
                    continue;
                }
                break;
            }
        }

        void skip_tag_tail() {
            bool in_quote = false;
            char quote = '\0';
            while (!eof()) {
                char c = peek();
                if (in_quote) {
                    next();
                    if (c == quote) in_quote = false;
                    continue;
                }
                if (c == '"' || c == '\'') {
                    in_quote = true;
                    quote = c;
                    next();
                    continue;
                }
                if (c == '>' || c == '/') break;
                next();
            }
        }

        std::pair<std::string, Value> parse_node() {
            consume('<');
            if (!eof() && peek() == '/') throw ParseError("Unexpected closing tag");
            std::string name;
            while (!eof()) {
                char c = peek();
                if (std::isspace(static_cast<unsigned char>(c)) || c == '/' || c == '>') break;
                name.push_back(next());
            }
            if (name.empty()) throw ParseError("Invalid XML tag name");
            skip_tag_tail();
            if (starts_with("/>")) { pos += 2; return {name, Value(nullptr)}; }
            consume('>');

            auto obj = std::make_shared<Object>();
            std::string text_node;
            while (!eof()) {
                if (starts_with("</")) break;
                if (starts_with("<!--")) { skip_misc(); continue; }
                if (starts_with("<![CDATA[")) {
                    pos += 9;
                    const auto e = s.find("]]>", pos);
                    if (e == std::string_view::npos) throw ParseError("Unterminated XML CDATA");
                    text_node.append(s.substr(pos, e - pos));
                    pos = e + 3;
                    continue;
                }
                if (starts_with("<?")) {
                    const auto e = s.find("?>", pos);
                    if (e == std::string_view::npos) throw ParseError("Unterminated XML processing instruction");
                    pos = e + 2;
                    continue;
                }
                if (peek() == '<') {
                    auto child = parse_node();
                    if (obj->contains(child.first)) {
                        Value& slot = obj->fields[child.first];
                        if (!slot.is_array()) {
                            auto arr = std::make_shared<Array>();
                            arr->push(slot);
                            slot = Value(arr);
                        }
                        slot.as_array().push(child.second);
                    } else {
                        obj->set(child.first, child.second);
                    }
                    continue;
                }
                text_node.push_back(next());
            }
            if (!starts_with("</")) throw ParseError("Missing XML closing tag");
            pos += 2;
            std::string close;
            while (!eof() && peek() != '>') close.push_back(next());
            consume('>');
            close = trim_copy(close);
            if (close != name) throw ParseError("XML tag mismatch");

            if (!obj->fields.empty()) return {name, Value(obj)};
            return {name, parse_scalar_token(trim_copy(text_node))};
        }
    };

    Parser p{text};
    p.skip_misc();
    auto root = std::make_shared<Object>();
    while (!p.eof()) {
        p.skip_misc();
        if (p.eof()) break;
        if (p.peek() != '<') throw ParseError("Invalid XML content");
        auto node = p.parse_node();
        if (root->contains(node.first)) {
            Value& slot = root->fields[node.first];
            if (!slot.is_array()) {
                auto arr = std::make_shared<Array>();
                arr->push(slot);
                slot = Value(arr);
            }
            slot.as_array().push(node.second);
        } else {
            root->set(node.first, node.second);
        }
    }
    Document d;
    d.root = Value(root);
    return d;
}

inline void emit_xml_node(std::ostringstream& out, const std::string& name, const Value& v) {
    if (v.is_object()) {
        out << "<" << name << ">";
        for (const auto& kv : v.as_object().fields) emit_xml_node(out, kv.first, kv.second);
        out << "</" << name << ">";
        return;
    }
    if (v.is_array()) {
        for (const auto& item : v.as_array().items) emit_xml_node(out, name, item);
        return;
    }
    out << "<" << name << ">";
    if (v.is_string()) out << v.as_string();
    else if (v.is_null()) out << "null";
    else if (v.is_bool()) out << (std::get<bool>(v.data) ? "true" : "false");
    else if (v.is_int()) out << std::get<std::int64_t>(v.data);
    else if (v.is_uint()) out << std::get<std::uint64_t>(v.data);
    else if (v.is_double()) out << std::get<double>(v.data);
    else out << "<binary>";
    out << "</" << name << ">";
}

inline Document parse_sexpr_basic(std::string_view text) {
    auto root = std::make_shared<Array>();
    std::vector<Array*> stack;
    stack.push_back(root.get());
    std::string tok;
    const auto flush_tok = [&]() {
        if (!tok.empty()) {
            stack.back()->push(parse_scalar_token(tok));
            tok.clear();
        }
    };
    for (char c : std::string(text)) {
        if (std::isspace(static_cast<unsigned char>(c))) { flush_tok(); continue; }
        if (c == '(') {
            flush_tok();
            auto child = std::make_shared<Array>();
            stack.back()->push(Value(child));
            stack.push_back(child.get());
            continue;
        }
        if (c == ')') {
            flush_tok();
            if (stack.size() > 1) stack.pop_back();
            continue;
        }
        tok.push_back(c);
    }
    flush_tok();
    Document d;
    if (root->items.size() == 1 && root->items[0].is_array()) d.root = root->items[0];
    else d.root = Value(root);
    return d;
}

inline void emit_sexpr_value(std::ostringstream& out, const Value& v) {
    if (v.is_array()) {
        out << "(";
        bool first = true;
        for (const auto& item : v.as_array().items) {
            if (!first) out << " ";
            emit_sexpr_value(out, item);
            first = false;
        }
        out << ")";
        return;
    }
    if (v.is_object()) {
        out << "(";
        bool first = true;
        for (const auto& kv : v.as_object().fields) {
            if (!first) out << " ";
            out << "(" << kv.first << " ";
            emit_sexpr_value(out, kv.second);
            out << ")";
            first = false;
        }
        out << ")";
        return;
    }
    if (v.is_string()) { out << "\"" << v.as_string() << "\""; return; }
    if (v.is_null()) { out << "nil"; return; }
    if (v.is_bool()) { out << (std::get<bool>(v.data) ? "true" : "false"); return; }
    if (v.is_int()) { out << std::get<std::int64_t>(v.data); return; }
    if (v.is_uint()) { out << std::get<std::uint64_t>(v.data); return; }
    if (v.is_double()) { out << std::get<double>(v.data); return; }
    out << "<binary>";
}
} // namespace detail

inline Document CompiledFormat::load_file(const std::filesystem::path& path) const { return is_textual() ? load_string(read_text_file(path)) : load_bytes(read_binary_file(path)); }
inline Document CompiledFormat::load_string(std::string_view input) const {
    if (!load_string_fn) throw FormatError("No textual loader for format: " + name);
    auto doc = load_string_fn(input); doc.source_format = name; return doc;
}
inline Document CompiledFormat::load_bytes(const std::vector<std::uint8_t>& bytes) const {
    if (!load_bytes_fn) throw FormatError("No binary loader for format: " + name);
    auto doc = load_bytes_fn(bytes); doc.source_format = name; return doc;
}
inline std::string CompiledFormat::dump_string(const Document& doc, const formatting::Options& opt) const {
    if (!dump_string_fn) throw FormatError("No textual dumper for format: " + name);
    return dump_string_fn(doc, opt);
}
inline std::vector<std::uint8_t> CompiledFormat::dump_bytes(const Document& doc) const {
    if (!dump_bytes_fn) throw FormatError("No binary dumper for format: " + name);
    return dump_bytes_fn(doc);
}
inline void CompiledFormat::dump_file(const Document& doc, const std::filesystem::path& path, const formatting::Options& opt) const {
    if (is_textual()) write_text_file(path, dump_string(doc, opt));
    else write_binary_file(path, dump_bytes(doc));
}

namespace builtins {
inline CompiledFormat json() {
    CompiledFormat fmt("JSON");
    fmt.category = FormatCategory::Textual;
    fmt.extensions = {".json"};
    fmt.load_string_fn = [](std::string_view text) {
        detail::JsonParser parser(text);
        Document doc;
        doc.root = parser.parse_value();
        parser.expect_end();
        return doc;
    };
    fmt.dump_string_fn = [](const Document& doc, const formatting::Options& opt) {
        std::ostringstream out;
        detail::emit_json_value(out, doc.root, opt, 0);
        if (opt.trailing_newline) out << opt.newline;
        return out.str();
    };
    return fmt;
}
inline CompiledFormat yaml() { CompiledFormat fmt("YAML"); fmt.category = FormatCategory::Textual; fmt.extensions = {".yaml", ".yml"}; fmt.load_string_fn = [](std::string_view t){ return detail::parse_yaml_basic(t); }; fmt.dump_string_fn = [](const Document& d, const formatting::Options&){ std::ostringstream out; detail::emit_yaml_value(out, d.root, 0); return out.str(); }; return fmt; }
inline CompiledFormat xml() { CompiledFormat fmt("XML"); fmt.category = FormatCategory::Textual; fmt.extensions = {".xml"}; fmt.load_string_fn = [](std::string_view t){ return detail::parse_xml_basic(t); }; fmt.dump_string_fn = [](const Document& d, const formatting::Options&){ std::ostringstream out; if (d.root.is_object()) { for (const auto& kv : d.root.as_object().fields) detail::emit_xml_node(out, kv.first, kv.second); } else { detail::emit_xml_node(out, "root", d.root); } return out.str(); }; return fmt; }
inline CompiledFormat sexpr() { CompiledFormat fmt("S-Expr"); fmt.category = FormatCategory::Textual; fmt.extensions = {".sexpr", ".scm", ".lisp"}; fmt.load_string_fn = [](std::string_view t){ return detail::parse_sexpr_basic(t); }; fmt.dump_string_fn = [](const Document& d, const formatting::Options&){ std::ostringstream out; detail::emit_sexpr_value(out, d.root); return out.str(); }; return fmt; }
inline CompiledFormat messagepack() { CompiledFormat fmt("MessagePack"); fmt.category = FormatCategory::Binary; fmt.extensions = {".msgpack", ".mpack"}; fmt.load_bytes_fn = [](const std::vector<std::uint8_t>& b){ if (b.size() >= 4 && b[0]=='S' && b[1]=='T' && b[2]=='K' && b[3]=='J') return builtins::json().load_string(std::string(reinterpret_cast<const char*>(b.data()+4), b.size()-4)); Document d; d.root = Value(Binary{b}); return d; }; fmt.dump_bytes_fn = [](const Document& d){ if (d.root.is_binary()) return d.root.as_binary().bytes; const auto s = builtins::json().dump_string(d); std::vector<std::uint8_t> out{'S','T','K','J'}; out.insert(out.end(), s.begin(), s.end()); return out; }; return fmt; }
inline CompiledFormat bson() { CompiledFormat fmt("BSON"); fmt.category = FormatCategory::Binary; fmt.extensions = {".bson"}; fmt.load_bytes_fn = [](const std::vector<std::uint8_t>& b){ if (b.size() >= 4 && b[0]=='S' && b[1]=='T' && b[2]=='K' && b[3]=='J') return builtins::json().load_string(std::string(reinterpret_cast<const char*>(b.data()+4), b.size()-4)); Document d; d.root = Value(Binary{b}); return d; }; fmt.dump_bytes_fn = [](const Document& d){ if (d.root.is_binary()) return d.root.as_binary().bytes; const auto s = builtins::json().dump_string(d); std::vector<std::uint8_t> out{'S','T','K','J'}; out.insert(out.end(), s.begin(), s.end()); return out; }; return fmt; }
} // namespace builtins

namespace sktl {
inline void validate(const Descriptor& d) {
    static const std::unordered_map<std::string, std::vector<std::string>> required{
        {"textual", {"identity", "model", "lexical", "parse", "dump", "query", "validation", "conversion"}},
        {"binary", {"identity", "model", "binary-layout", "parse", "dump", "query", "validation", "conversion", "binarization"}},
    };
    static const std::unordered_set<std::string> known{
        "identity", "model", "lexical", "binary-layout", "parse", "dump", "formatting",
        "query", "validation", "conversion", "binarization", "__top_mime__", "__top_encoding__"
    };
    const char* cat = d.category == FormatCategory::Binary ? "binary" : "textual";
    for (const auto& [section, _] : d.sections) {
        if (!known.contains(section)) throw ParseError("Invalid SKTL section in " + d.name + ": " + section);
    }
    const auto it = required.find(cat);
    if (it != required.end()) {
        for (const auto& section : it->second) {
            if (!d.sections.contains(section)) throw ParseError("Missing SKTL section in " + d.name + ": " + section);
        }
    }
}

inline std::vector<std::string> split_tokens_after_key(const std::vector<std::string>& lines, const std::string& key) {
    std::vector<std::string> out;
    for (const auto& line : lines) {
        std::istringstream ls(line);
        std::string tok;
        ls >> tok;
        if (tok != key) continue;
        while (ls >> tok) out.push_back(tok);
    }
    return out;
}

inline CompiledFormat compile(const Descriptor& d) {
    validate(d);
    CompiledFormat fmt;
    if (d.name == "JSON") fmt = builtins::json();
    else if (d.name == "YAML") fmt = builtins::yaml();
    else if (d.name == "XML") fmt = builtins::xml();
    else if (d.name == "S-Expr") fmt = builtins::sexpr();
    else if (d.name == "MessagePack") fmt = builtins::messagepack();
    else if (d.name == "BSON") fmt = builtins::bson();
    else throw FormatError("No builtin compiler backend for SKTL format: " + d.name);

    fmt.name = d.name;
    fmt.category = d.category;
    if (!d.extensions.empty()) fmt.extensions = d.extensions;
    fmt.sktl_sections = d.sections;
    if (auto it = d.sections.find("__top_mime__"); it != d.sections.end()) fmt.mime_types = it->second;
    if (auto it = d.sections.find("__top_encoding__"); it != d.sections.end() && !it->second.empty()) fmt.encoding = it->second.front();
    if (auto it = d.sections.find("query"); it != d.sections.end()) {
        auto q = split_tokens_after_key(it->second, "engines");
        if (!q.empty()) fmt.query_engines = std::move(q);
    }
    if (auto it = d.sections.find("validation"); it != d.sections.end()) {
        auto v = split_tokens_after_key(it->second, "standards");
        if (!v.empty()) fmt.validation_standards = std::move(v);
    }
    if (auto it = d.sections.find("conversion"); it != d.sections.end()) {
        auto a = split_tokens_after_key(it->second, "adapters");
        if (!a.empty()) fmt.conversion_adapters = std::move(a);
    }
    return fmt;
}
} // namespace sktl

inline void register_builtin_formats(FormatRegistry& registry = FormatRegistry::instance()) {
    registry.register_format(builtins::json());
    registry.register_format(builtins::yaml());
    registry.register_format(builtins::xml());
    registry.register_format(builtins::sexpr());
    registry.register_format(builtins::messagepack());
    registry.register_format(builtins::bson());
}

template <typename Adapter>
struct Convert {
    static Document run(const Document& input, conversion::Report* report = nullptr, conversion::Policy = conversion::Policy::BestEffort) {
        if (report) {
            report->warnings.push_back("Adapter conversion path used");
            report->losses.push_back(conversion::LossKind::Placeholder);
        }
        return Adapter::from_common(Adapter::to_common(input, report), report);
    }
};

template <typename Adapter>
struct Binarize {
    static Document run(const Document& input, conversion::Report* report = nullptr, conversion::Policy p = conversion::Policy::LossAware) {
        return Convert<Adapter>::run(input, report, p);
    }
};

template <typename Engine>
struct Query {
    static query::Result run(const Document& doc, std::string_view expression) { return Engine::run(doc, expression); }
};

namespace facade {
template <const CompiledFormat& (*FormatFn)()>
struct API {
    static const CompiledFormat& format() { return FormatFn(); }
    template <typename MemoryPolicy = memory::Manual>
    class Creator : public serdetk::Creator<MemoryPolicy> { public: Creator() : serdetk::Creator<MemoryPolicy>(&FormatFn()) {} };
    class SimplePrinter : public serdetk::SimplePrinter { public: SimplePrinter() : serdetk::SimplePrinter(&FormatFn()) {} };
    class PrettyPrinter : public serdetk::PrettyPrinter { public: PrettyPrinter(formatting::Options o = {}) : serdetk::PrettyPrinter(&FormatFn(), std::move(o)) {} };
    struct Minify { static std::string run(const Document& doc) { return serdetk::Minify::run(FormatFn(), doc); } };
    class Editor : public serdetk::Editor { public: explicit Editor(Document d) : serdetk::Editor(std::move(d), &FormatFn()) {} static Editor open(const std::filesystem::path& p) { return Editor(FormatFn().load_file(p)); } };
    class Validator : public serdetk::Validator {
    public:
        explicit Validator(Document s) : serdetk::Validator(std::move(s)) {}
        static std::shared_ptr<Validator> from_schema(std::string_view t) { return std::make_shared<Validator>(FormatFn().load_string(t)); }
        static std::shared_ptr<Validator> from_schema_file(const std::filesystem::path& p) { return std::make_shared<Validator>(FormatFn().load_file(p)); }
    };

    template <typename Engine> using Query = serdetk::Query<Engine>;
    template <typename Adapter> using Convert = serdetk::Convert<Adapter>;
    template <typename Adapter> using Binarize = serdetk::Binarize<Adapter>;

    static Document load_from_file(const std::filesystem::path& p) { return FormatFn().load_file(p); }
    static Document from_file(const std::filesystem::path& p) { return load_from_file(p); }
    static Document load_string(std::string_view s) { return FormatFn().load_string(s); }
    static Document from_string(std::string_view s) { return load_string(s); }
    static Document load_bytes(const std::vector<std::uint8_t>& b) { return FormatFn().load_bytes(b); }
    static Document from_bytes(const std::vector<std::uint8_t>& b) { return load_bytes(b); }
    static void dump_to_file(const Document& doc, const std::filesystem::path& p, const formatting::Options& opt = {}) { FormatFn().dump_file(doc, p, opt); }
};
} // namespace facade

namespace yaml { struct JSONAdapter { static Document to_common(const Document& in, conversion::Report*) { return in; } static Document from_common(const Document& in, conversion::Report*) { return in; } }; }
namespace xml { struct JSONAdapter { static Document to_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::AttributeLoss); return in; } static Document from_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::AttributeLoss); return in; } }; }
namespace bson { struct JSONAdapter { static Document to_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::BinaryCarrier); return in; } static Document from_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::BinaryCarrier); return in; } }; }
namespace msgpack { struct JSONAdapter { static Document to_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::BinaryCarrier); return in; } static Document from_common(const Document& in, conversion::Report* r) { if (r) r->losses.push_back(conversion::LossKind::BinaryCarrier); return in; } }; }

namespace json {
inline const CompiledFormat& format() { static CompiledFormat fmt = builtins::json(); return fmt; }
using _api = facade::API<format>;
template <typename M = memory::Manual> using Creator = _api::template Creator<M>;
using SimplePrinter = _api::SimplePrinter;
using PrettyPrinter = _api::PrettyPrinter;
using Minify = _api::Minify;
using Editor = _api::Editor;
using Validator = _api::Validator;
template <typename E> using Query = _api::template Query<E>;
template <typename A> using Convert = _api::template Convert<A>;
template <typename A> using Binarize = _api::template Binarize<A>;
inline Document load_from_file(const std::filesystem::path& p) { return _api::load_from_file(p); }
inline Document from_file(const std::filesystem::path& p) { return _api::from_file(p); }
inline Document load_string(std::string_view s) { return _api::load_string(s); }
inline Document load_bytes(const std::vector<std::uint8_t>& b) { return _api::load_bytes(b); }
inline Document from_string(std::string_view s) { return _api::from_string(s); }
inline Document from_bytes(const std::vector<std::uint8_t>& b) { return _api::from_bytes(b); }
inline void dump_to_file(const Document& d, const std::filesystem::path& p, const formatting::Options& o = {}) { _api::dump_to_file(d, p, o); }
inline std::string minify(const Document& doc) { return Minify::run(doc); }
} // namespace json

#define SERDETK_DEFINE_FORMAT_NS(ns_name, fn_name) \
namespace ns_name { \
inline const CompiledFormat& format() { static CompiledFormat fmt = builtins::fn_name(); return fmt; } \
using _api = facade::API<format>; \
template <typename M = memory::Manual> using Creator = _api::template Creator<M>; \
using SimplePrinter = _api::SimplePrinter; \
using PrettyPrinter = _api::PrettyPrinter; \
using Minify = _api::Minify; \
using Editor = _api::Editor; \
template <typename E> using Query = _api::template Query<E>; \
template <typename A> using Convert = _api::template Convert<A>; \
template <typename A> using Binarize = _api::template Binarize<A>; \
inline Document load_from_file(const std::filesystem::path& p) { return _api::load_from_file(p); } \
inline Document from_file(const std::filesystem::path& p) { return _api::from_file(p); } \
inline Document load_string(std::string_view s) { return _api::load_string(s); } \
inline Document load_bytes(const std::vector<std::uint8_t>& b) { return _api::load_bytes(b); } \
inline Document from_string(std::string_view s) { return _api::from_string(s); } \
inline Document from_bytes(const std::vector<std::uint8_t>& b) { return _api::from_bytes(b); } \
inline void dump_to_file(const Document& d, const std::filesystem::path& p, const formatting::Options& o = {}) { _api::dump_to_file(d, p, o); } \
}

SERDETK_DEFINE_FORMAT_NS(yaml, yaml)
SERDETK_DEFINE_FORMAT_NS(xml, xml)
SERDETK_DEFINE_FORMAT_NS(sexpr, sexpr)
SERDETK_DEFINE_FORMAT_NS(msgpack, messagepack)
SERDETK_DEFINE_FORMAT_NS(bson, bson)

#undef SERDETK_DEFINE_FORMAT_NS

using JQ = query::JQ;
using STKQ = query::STKQ;
using SPARQL = query::SPARQL;

} // namespace serdetk

#endif // SERDETK_HPP
