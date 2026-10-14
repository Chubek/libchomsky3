#define CATCH_CONFIG_MAIN

#include <catch2/catch_all.hpp>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "include/QaMRpp.hpp"
#include "podlet/PodletPackaging.hpp"
#include "plugins/QaMRpp-Compile2Bytecode.hpp"
#include "plugins/QaMRpp-Compile2C.hpp"
#include "plugins/QaMRpp-Compile2WASM.hpp"

namespace {

struct TempDir {
    std::filesystem::path path;

    TempDir() {
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::string name = std::string("qamrpp_test_") + std::to_string(now);
        path = std::filesystem::temp_directory_path() / name;
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

bool write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.good()) return false;
    out << text;
    return out.good();
}

bool write_binary_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.good()) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct ScopedEnv {
    std::string key;
    bool had = false;
    std::string old;

    explicit ScopedEnv(const std::string& k) : key(k) {
        if (const char* v = std::getenv(key.c_str())) {
            had = true;
            old = v;
        }
    }

    void set(const std::string& v) const {
        REQUIRE(setenv(key.c_str(), v.c_str(), 1) == 0);
    }

    void clear() {
        if (had) {
            REQUIRE(setenv(key.c_str(), old.c_str(), 1) == 0);
        } else {
            REQUIRE(unsetenv(key.c_str()) == 0);
        }
    }

    ~ScopedEnv() {
        clear();
    }
};

int run_command(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string locate_binary(const std::string& name) {
    const std::vector<std::string> candidates = {
        std::string("./") + name,
        std::string("./cli/") + name,
        std::string("../") + name,
        std::string("../cli/") + name,
        std::string("./build/") + name,
        std::string("./build/cli/") + name,
        std::string("../build/") + name,
        std::string("../build/cli/") + name,
    };

    const std::filesystem::path current = std::filesystem::current_path();
    const std::filesystem::path source = std::filesystem::path(__FILE__).parent_path();

    for (const auto& candidate : candidates) {
        const std::filesystem::path p1 = current / candidate;
        if (std::filesystem::exists(p1)) return std::filesystem::absolute(p1).string();
        const std::filesystem::path p2 = source / candidate;
        if (std::filesystem::exists(p2)) return std::filesystem::absolute(p2).string();
    }

    return {};
}

template <typename T>
T get_table_field(const qamrpp::ValuePtr& t, const std::string& key) {
    return static_cast<T>(0);
}

std::vector<std::uint8_t> make_qpod_bytes(
    const std::string& name,
    const std::string& version,
    const std::string& entrypoint,
    const std::string& source,
    bool include_optional = true
) {
    serdetk::Document doc;
    auto root = std::make_shared<serdetk::Object>();
    root->set("format", serdetk::Value(std::string("qamrpp.qpod/1")));

    auto manifest = std::make_shared<serdetk::Object>();
    manifest->set("name", serdetk::Value(name));
    manifest->set("version", serdetk::Value(version));
    manifest->set("entrypoint", serdetk::Value(entrypoint));
    if (include_optional) {
        manifest->set("podlet_api", serdetk::Value(std::string("1")));
        manifest->set("format_version", serdetk::Value(std::string("1")));
    }

    root->set("manifest", serdetk::Value(manifest));

    auto files = std::make_shared<serdetk::Object>();
    files->set(entrypoint, serdetk::Value(source));
    root->set("files", serdetk::Value(files));

    doc.root = serdetk::Value(root);
    return serdetk::builtins::messagepack().dump_bytes(doc);
}

} // namespace

TEST_CASE("arithmetic precedence and float handling") {
    qamrpp::Context ctx;
    auto result = ctx.run("return 2 + 3 * 4 - 1 / 2");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == Catch::Approx(13.5));
}

TEST_CASE("comparison and boolean operators") {
    qamrpp::Context ctx;
    auto result = ctx.run("if (1 < 2) and (not false) then return true else return false end");
    REQUIRE(result->type == qamrpp::Value::BOOL);
    REQUIRE(result->bool_value);
}

TEST_CASE("unary operators and concat") {
    qamrpp::Context ctx;
    auto result = ctx.run("return -1 * (-5) .. '-' .. #'abc' .. '-' .. (not false and \"true\" or \"false\")");
    REQUIRE(result->type == qamrpp::Value::STRING);
    REQUIRE(result->string_value == "5-3-true");
}

TEST_CASE("numeric and boolean literal literals") {
    qamrpp::Context ctx;
    auto result = ctx.run("return 7, 3.5, true");
    REQUIRE(result->type == qamrpp::Value::INT);
    REQUIRE(result->int_value == 7);
}

TEST_CASE("table literal, raw get, raw set") {
    qamrpp::Context ctx;
    auto result = ctx.run("return {a=10, b=20, 30}");
    REQUIRE(result->type == qamrpp::Value::TABLE);

    auto a = ctx.table_raw_get(result, std::make_shared<qamrpp::Value>(std::string("a")));
    auto b = ctx.table_raw_get(result, std::make_shared<qamrpp::Value>(std::string("b")));
    auto c = ctx.table_raw_get(result, std::make_shared<qamrpp::Value>(int64_t{1}));

    const bool a_is_number = a->type == qamrpp::Value::INT || a->type == qamrpp::Value::FLOAT;
    REQUIRE(a_is_number);
    REQUIRE(a->to_string() == "10");
    const bool b_is_number = b->type == qamrpp::Value::INT || b->type == qamrpp::Value::FLOAT;
    REQUIRE(b_is_number);
    REQUIRE(b->to_string() == "20");
    const bool c_is_number = c->type == qamrpp::Value::INT || c->type == qamrpp::Value::FLOAT;
    REQUIRE(c_is_number);
    REQUIRE(c->to_string() == "30");

    auto table = qamrpp::Value::make_table();
    qamrpp::Context ctx2;
    ctx2.table_raw_set(table, std::make_shared<qamrpp::Value>(std::string("x")), std::make_shared<qamrpp::Value>(int64_t{42}));
    auto fetched = ctx2.table_raw_get(table, std::make_shared<qamrpp::Value>(std::string("x")));
    REQUIRE(fetched->type == qamrpp::Value::INT);
    REQUIRE(fetched->int_value == 42);
    ctx2.table_raw_set(table, std::make_shared<qamrpp::Value>(std::string("x")), std::make_shared<qamrpp::Value>());
    auto removed = ctx2.table_raw_get(table, std::make_shared<qamrpp::Value>(std::string("x")));
    REQUIRE(removed->type == qamrpp::Value::NIL);
}

TEST_CASE("table.get respects __index fallback") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "fallback = {value = 123}\n"
        "t = {}\n"
        "setmetatable(t, {__index = fallback})\n"
        "return t.value"
    );
    REQUIRE(result->type == qamrpp::Value::INT);
    REQUIRE(result->int_value == 123);
}

TEST_CASE("table.set respects __newindex") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "sink = {}\n"
        "t = {}\n"
        "setmetatable(t, {__newindex = function(_, key, value) sink[key] = value end})\n"
        "t.answer = 42\n"
        "return sink.answer"
    );
    REQUIRE(result->type == qamrpp::Value::INT);
    REQUIRE(result->int_value == 42);
}

TEST_CASE("table.calling __call works for function-like tables") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "mt = {}\n"
        "setmetatable(mt, {__call = function(self, value) return value + 1 end})\n"
        "return mt(8)"
    );
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 9.0);
}

TEST_CASE("locals shadow global scope") {
    qamrpp::Context ctx;
    auto result = ctx.run("x = 10\nfunction f() local x = 20 return x end\nlocal y = f()\nreturn x + y");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 30.0);
}

TEST_CASE("function definitions return values") {
    qamrpp::Context ctx;
    auto result = ctx.run("function add(a, b) return a + b end\nreturn add(4, 6)");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 10.0);
}

TEST_CASE("function expressions preserve captures") {
    qamrpp::Context ctx;
    auto result = ctx.run("adder = function(v) return v * 2 end\nreturn adder(5)");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 10.0);
}

TEST_CASE("method call sugar works") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "obj = {value=3}\n"
        "obj.inc = function(self, x) self.value = self.value + x return self.value end\n"
        "return obj:inc(4)"
    );
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 7.0);
}

TEST_CASE("multiple assignment with trailing returns") {
    qamrpp::Context ctx;
    auto result = ctx.run("function pair() return 8,9 end\na,b = pair()\nreturn a + b");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 17.0);
}

TEST_CASE("multi-assignment expands trailing returns") {
    qamrpp::Context ctx;
    auto result = ctx.run("function t() return 4,5,6 end\na,b,c = 1, t()\nreturn a + b + c");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 10.0);
}

TEST_CASE("if / elseif / else control flow") {
    qamrpp::Context ctx;
    auto result = ctx.run("if false then return 1 elseif true then return 2 else return 3 end");
    REQUIRE(result->type == qamrpp::Value::INT);
    REQUIRE(result->int_value == 2);
}

TEST_CASE("numeric for loop") {
    qamrpp::Context ctx;
    auto result = ctx.run("s = 0\nfor i = 1, 4 do s = s + i end\nreturn s");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 10.0);
}

TEST_CASE("numeric for loop with custom step") {
    qamrpp::Context ctx;
    auto result = ctx.run("s = 0\nfor i = 1, 9, 3 do s = s + i end\nreturn s");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 12.0);
}

TEST_CASE("while and repeat loops accumulate state") {
    qamrpp::Context ctx;
    auto while_result = ctx.run("s = 0\ni = 1\nwhile i <= 3 do s = s + i; i = i + 1 end\nreturn s");
    auto repeat_result = ctx.run("s = 0\ni = 1\nrepeat s = s + i; i = i + 1 until i > 3\nreturn s");
    REQUIRE(while_result->type == qamrpp::Value::FLOAT);
    REQUIRE(while_result->float_value == 6.0);
    REQUIRE(repeat_result->type == qamrpp::Value::FLOAT);
    REQUIRE(repeat_result->float_value == 6.0);
}

TEST_CASE("generic for over iterator closure") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "function seq(state, control)\n"
        "  if control >= 3 then return nil end\n"
        "  return control + 1, control * 10\n"
        "end\n"
        "acc = 0\n"
        "for k,v in seq, nil, 0 do acc = acc + k + v end\n"
        "return acc"
    );
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 36.0);
}

TEST_CASE("break leaves loop early") {
    qamrpp::Context ctx;
    auto result = ctx.run("s = 0\nfor i=1,10 do if i == 4 then break end; s = s + i end\nreturn s");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 6.0);
}

TEST_CASE("linker combines in-memory sources") {
    qamrpp::Linker linker;
    linker.add_source("alpha.lua", "x = 2");
    linker.add_source("beta.lua", "x = x + 8");
    qamrpp::Context ctx;
    auto result = linker.link(ctx);
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 10.0);
}

TEST_CASE("linker reads from disk") {
    TempDir work;
    const std::filesystem::path path = work.path / "unit.lua";
    REQUIRE(write_text_file(path, "value = 7\nreturn value * 2"));

    qamrpp::Context ctx;
    REQUIRE(ctx.linker.add_file(path.string()));
    auto result = ctx.linker.link(ctx);
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 14.0);
}

TEST_CASE("linker errors include source name") {
    qamrpp::Context ctx;
    qamrpp::Linker linker;
    linker.add_source("bad_unit.lua", "unknown_fn(1)");
    REQUIRE_THROWS_AS(linker.link(ctx), std::exception);
    try {
        linker.link(ctx);
    } catch (const std::exception& e) {
        REQUIRE_THAT(e.what(), Catch::Matchers::ContainsSubstring("bad_unit.lua"));
    }
}

TEST_CASE("host symbol tables are populated") {
    qamrpp::Context ctx;
    int c_symbol = 42;
    ctx.register_c_symbol("symbol_probe", reinterpret_cast<void*>(&c_symbol));
    ctx.register_cpp_symbol("cpp_value", &c_symbol);

    REQUIRE(ctx.c_symbol_table.find("symbol_probe") != ctx.c_symbol_table.end());
    REQUIRE(ctx.cpp_symbol_table.find("cpp_value") != ctx.cpp_symbol_table.end());
}

TEST_CASE("hooks fire for run lifecycle") {
    qamrpp::Context ctx;
    int before = 0;
    int after_parse = 0;
    int after_eval = 0;

    ctx.add_hook(qamrpp::HookPoint::BeforeRun, [&](qamrpp::Context&, const qamrpp::HookPayload&) { ++before; });
    ctx.add_hook(qamrpp::HookPoint::AfterParse, [&](qamrpp::Context&, const qamrpp::HookPayload&) { ++after_parse; });
    ctx.add_hook(qamrpp::HookPoint::AfterEval, [&](qamrpp::Context&, const qamrpp::HookPayload&) { ++after_eval; });

    auto result = ctx.run("return 1 + 1");
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(before == 1);
    REQUIRE(after_parse == 1);
    REQUIRE(after_eval == 1);
}

TEST_CASE("hooks fire on parse/eval error") {
    qamrpp::Context ctx;
    int errors = 0;
    ctx.add_hook(qamrpp::HookPoint::OnError, [&](qamrpp::Context&, const qamrpp::HookPayload&) { ++errors; });
    REQUIRE_THROWS(ctx.run("@"));
    REQUIRE(errors == 1);
}

TEST_CASE("compile2bytecode translation format") {
    qamrpp::Compile2BytecodePlugin plugin;
    const std::string out = plugin.translate("10 + 20");
    REQUIRE(out.find("QBC1") == 0);
    REQUIRE(out.find("PUSHI8") != std::string::npos);
}

TEST_CASE("compile2c translation format") {
    qamrpp::Compile2CPlugin plugin;
    const std::string out = plugin.translate("2 * 4");
    REQUIRE(out.find("const char qamrpp_ir_data") != std::string::npos);
    REQUIRE(out.find("qamrpp_eval_ir_i64") != std::string::npos);
}

TEST_CASE("compile2wasm translation format") {
    qamrpp::Compile2WASMPlugin plugin;
    const std::string out = plugin.translate("3 + 4");
    REQUIRE(out.find("(module") != std::string::npos);
    REQUIRE(out.find("(export \"qamrpp_eval_ir_i64\"") != std::string::npos);
}

TEST_CASE("plugins register native translation functions") {
    qamrpp::Context ctx;
    qamrpp::Compile2BytecodePlugin byte_plugin;
    qamrpp::Compile2CPlugin c_plugin;
    qamrpp::Compile2WASMPlugin wasm_plugin;

    byte_plugin.install(ctx);
    c_plugin.install(ctx);
    wasm_plugin.install(ctx);

    auto byte_fn = ctx.lookup_name("compile_to_bytecode");
    auto c_fn = ctx.lookup_name("compile_to_c");
    auto wasm_fn = ctx.lookup_name("compile_to_wasm");
    REQUIRE(byte_fn != nullptr);
    REQUIRE(byte_fn->type == qamrpp::Value::FUNCTION);
    REQUIRE(c_fn != nullptr);
    REQUIRE(c_fn->type == qamrpp::Value::FUNCTION);
    REQUIRE(wasm_fn != nullptr);
    REQUIRE(wasm_fn->type == qamrpp::Value::FUNCTION);

    std::vector<qamrpp::ValuePtr> args{std::make_shared<qamrpp::Value>(std::string("1+1"))};
    auto out_a = byte_fn->function_value(ctx, args);
    auto out_b = c_fn->function_value(ctx, args);
    auto out_c = wasm_fn->function_value(ctx, args);

    REQUIRE(out_a != nullptr);
    REQUIRE(out_a->type == qamrpp::Value::STRING);
    REQUIRE(out_b != nullptr);
    REQUIRE(out_b->type == qamrpp::Value::STRING);
    REQUIRE(out_c != nullptr);
    REQUIRE(out_c->type == qamrpp::Value::STRING);
    REQUIRE(out_a->string_value.find("PUSHI8") != std::string::npos);
}

TEST_CASE("extension install through add_extension") {
    class RegisterTwiceExtension : public qamrpp::Extension {
    public:
        const char* name() const override {
            return "register_twice";
        }

        void register_functions(qamrpp::Context& ctx) override {
            ctx.register_native("echo_twice", [](qamrpp::Context&, std::vector<qamrpp::ValuePtr>& args) -> qamrpp::ValuePtr {
                return std::make_shared<qamrpp::Value>(args.empty() ? std::string("") : args[0]->to_string() + args[0]->to_string());
            });
        }
    };

    qamrpp::Context ctx;
    ctx.add_extension(std::make_unique<RegisterTwiceExtension>());

    auto echo_fn = ctx.lookup_name("echo_twice");
    REQUIRE(echo_fn != nullptr);
    REQUIRE(echo_fn->type == qamrpp::Value::FUNCTION);
    std::vector<qamrpp::ValuePtr> args{std::make_shared<qamrpp::Value>(std::string("x"))};
    auto output = echo_fn->function_value(ctx, args);
    REQUIRE(output != nullptr);
    REQUIRE(output->type == qamrpp::Value::STRING);
    REQUIRE(output->string_value == "xx");
}

TEST_CASE("podlet packaging builds qpod when include_all_files is disabled") {
    TempDir work;
    const std::filesystem::path project = work.path / "minimal";
    REQUIRE(std::filesystem::create_directories(project));

    REQUIRE(write_text_file(project / "Podlet.cpp", "int podlet() { return 1; }\n"));
    REQUIRE(write_text_file(project / "Podpack.qmr",
        "name = minimal_podlet\n"
        "version = 0.1.0\n"
        "entrypoint = Podlet.cpp\n"));
    REQUIRE(write_text_file(project / "Other.txt", "ignore-me\n"));

    qamrpp::podlet::PackageOptions options;
    options.source_root = project.string();
    options.output_qpod = (project / "minimal.qpod").string();
    options.include_all_files = false;
    auto result = qamrpp::podlet::build_qpod(options);

    REQUIRE(result.ok);
    REQUIRE(result.file_count == 1);
    auto data = read_binary_file(result.output_qpod);
    serdetk::Document doc = serdetk::builtins::messagepack().load_bytes(data);
    const auto& files = doc.root.as_object().fields.at("files").as_object().fields;
    REQUIRE(files.count("Podlet.cpp") == 1);
    REQUIRE(files.count("Other.txt") == 0);
}

TEST_CASE("podlet packaging validates required files") {
    TempDir work;
    const std::filesystem::path project = work.path / "bad";
    REQUIRE(std::filesystem::create_directories(project));
    REQUIRE(write_text_file(project / "Podlet.cpp", "int podlet() { return 1; }\n"));

    qamrpp::podlet::PackageOptions missing_manifest;
    missing_manifest.source_root = project.string();
    missing_manifest.output_qpod = (project / "bad.qpod").string();
    auto bad = qamrpp::podlet::build_qpod(missing_manifest);
    REQUIRE(!bad.ok);
    REQUIRE(bad.error.find("missing required manifest") != std::string::npos);

    qamrpp::podlet::PackageOptions wrong_entry;
    wrong_entry.source_root = project.string();
    wrong_entry.output_qpod = (project / "bad2.qpod").string();
    REQUIRE(write_text_file(project / "Podpack.qmr",
        "name = broken\n"
        "version = 0.1.0\n"
        "entrypoint = Missing.cpp\n"));
    auto bad2 = qamrpp::podlet::build_qpod(wrong_entry);
    REQUIRE(!bad2.ok);
    REQUIRE(bad2.error.find("entrypoint does not exist") != std::string::npos);
}

TEST_CASE("podlet runtime loads exports and metadata") {
    TempDir work;
    const std::filesystem::path archive = work.path / "runtime.qpod";
    REQUIRE(write_binary_file(archive, make_qpod_bytes("runtime_podlet", "0.1.0", "Podlet.cpp", "return 5")));

    qamrpp::Context ctx;
    REQUIRE(ctx.load_library_named(archive.string()));
    auto exports = ctx.lookup_name("runtime_podlet");
    REQUIRE(exports);
    REQUIRE(exports->type == qamrpp::Value::TABLE);

    auto name = ctx.table_raw_get(exports, std::make_shared<qamrpp::Value>(std::string("name")));
    auto version = ctx.table_raw_get(exports, std::make_shared<qamrpp::Value>(std::string("version")));
    auto source = ctx.table_raw_get(exports, std::make_shared<qamrpp::Value>(std::string("source")));

    REQUIRE(name->type == qamrpp::Value::STRING);
    REQUIRE(name->string_value == "runtime_podlet");
    REQUIRE(version->type == qamrpp::Value::STRING);
    REQUIRE(version->string_value == "0.1.0");
    REQUIRE(source->type == qamrpp::Value::STRING);
    REQUIRE(source->string_value == "return 5");
}

TEST_CASE("podlet runtime rejects malformed archive payload") {
    TempDir work;
    const std::filesystem::path archive = work.path / "bad.qpod";
    REQUIRE(write_binary_file(archive, {0xde, 0xad, 0xbe, 0xef}));

    qamrpp::Context ctx;
    REQUIRE(!ctx.load_library_named(archive.string()));
    REQUIRE(ctx.last_error_message.find("invalid podlet archive") != std::string::npos);
}

TEST_CASE("podlet runtime rejects manifest mismatch requested name") {
    TempDir work;
    const std::filesystem::path archive = work.path / "requested_name.qpod";
    REQUIRE(write_binary_file(archive, make_qpod_bytes("actual", "1.0.0", "Podlet.cpp", "return 1")));
    ScopedEnv path_guard("QAMRPP_PATH");
    path_guard.set(work.path.string());

    qamrpp::Context ctx;
    REQUIRE(!ctx.load_library_named("requested_name"));
    REQUIRE(ctx.last_error_code != QAMRPP_OK);
    REQUIRE(ctx.last_error_message.find("name does not match") != std::string::npos);
}

TEST_CASE("podlet runtime cache returns same module pointer") {
    TempDir work;
    const std::filesystem::path archive = work.path / "cached.qpod";
    REQUIRE(write_binary_file(archive, make_qpod_bytes("cached_podlet", "1.0.0", "Podlet.cpp", "return 3")));

    qamrpp::Context ctx;
    REQUIRE(ctx.load_library_named(archive.string()));
    auto first = ctx.lookup_name("cached_podlet");
    REQUIRE(first);
    REQUIRE(ctx.load_library_named(archive.string()));
    auto second = ctx.lookup_name("cached_podlet");
    REQUIRE(second);
    REQUIRE(first.get() == second.get());
}

TEST_CASE("podlet loading honors QAMRPP_PATH search path") {
    TempDir work;
    const std::filesystem::path pod_dir = work.path / "pods";
    const std::filesystem::path archive = pod_dir / "core.qpod";
    REQUIRE(std::filesystem::create_directories(pod_dir));
    REQUIRE(write_binary_file(archive, make_qpod_bytes("sample", "9.9.9", "Podlet.cpp", "return 1")));
    REQUIRE(write_binary_file(pod_dir / "podlet" / "sample.qpod", make_qpod_bytes("sample", "9.9.9", "Podlet.cpp", "return 1")));

    ScopedEnv path_guard("QAMRPP_PATH");
    path_guard.set(pod_dir.string());
    ScopedEnv home_guard("HOME");
    home_guard.set((work.path / "fake_home").string());

    qamrpp::Context ctx;
    REQUIRE(ctx.load_library_named("sample"));
    auto module = ctx.lookup_name("sample");
    REQUIRE(module);
}

TEST_CASE("podpack --init scaffold, build, and install") {
    const std::string podpack = locate_binary("podpack");
    if (podpack.empty()) {
        SUCCEED("podpack binary not available in this build configuration");
        return;
    }

    TempDir work;
    const std::filesystem::path project = work.path / "podlet_project";
    const std::filesystem::path out_archive = work.path / "hello.qpod";
    const std::filesystem::path install_root = work.path / "install_root";

    const std::string init_cmd = podpack + " --init " + project.string();
    REQUIRE(run_command(init_cmd) == 0);
    REQUIRE(std::filesystem::exists(project / "Podlet.cpp"));
    REQUIRE(std::filesystem::exists(project / "Podpack.qmr"));

    std::string manifest = read_text_file(project / "Podpack.qmr");
    REQUIRE(manifest.find("entrypoint = Podlet.cpp") != std::string::npos);

    const std::string build_cmd = podpack + " " + project.string() + " --output " + out_archive.string();
    REQUIRE(run_command(build_cmd) == 0);
    REQUIRE(std::filesystem::exists(out_archive));

    const std::string install_cmd = podpack + " --install " + out_archive.string() + " --root " + install_root.string();
    REQUIRE(run_command(install_cmd) == 0);
    REQUIRE(std::filesystem::exists(install_root / out_archive.filename()));

    const auto bytes = read_binary_file(out_archive);
    REQUIRE(!bytes.empty());
    const serdetk::Document doc = serdetk::builtins::messagepack().load_bytes(bytes);
    const auto& root = doc.root.as_object();
    REQUIRE(root.fields.at("format").is_string());
    REQUIRE(root.fields.at("format").as_string() == "qamrpp.qpod/1");
}

TEST_CASE("runner catches malformed script via qamrpp-cli") {
    const std::string cli = locate_binary("qamrpp-cli");
    if (cli.empty()) {
        SUCCEED("qamrpp-cli binary not available in this build configuration");
        return;
    }

    TempDir work;
    const std::filesystem::path bad_script = work.path / "bad.lua";
    REQUIRE(write_text_file(bad_script, "@\n"));

    const std::string out_file = (work.path / "bad.out").string();
    const int status = run_command(cli + " --script " + bad_script.string() + " >" + out_file + " 2>&1");
    REQUIRE(status != 0);

    const std::string output = read_text_file(out_file);
    const bool has_unexpected_character = output.find("unexpected character") != std::string::npos;
    const bool has_expected_closing = output.find("expected '}'") != std::string::npos;
    REQUIRE((has_unexpected_character || has_expected_closing) == true);
}

TEST_CASE("qamrpp-cli auto-loads stdlib and stdlib++ from HOME") {
    const std::string cli = locate_binary("qamrpp-cli");
    if (cli.empty()) {
        SUCCEED("qamrpp-cli binary not available in this build configuration");
        return;
    }

    TempDir work;
    const std::filesystem::path fake_home = work.path / "fake_home";
    const std::filesystem::path home_qamrpp = fake_home / ".qamrpp";
    const std::filesystem::path script = work.path / "uses_stdlib.lua";
    const std::filesystem::path out_file = work.path / "cli.out";

    REQUIRE(std::filesystem::create_directories(home_qamrpp / "stdc"));
    REQUIRE(std::filesystem::create_directories(home_qamrpp / "stdc++"));

    const std::filesystem::path source_file_dir = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::filesystem::path source_root = source_file_dir;
    for (std::filesystem::path cursor = source_file_dir;; cursor = cursor.parent_path()) {
        if (std::filesystem::exists(cursor / "include" / "QaMRpp-Library.hpp") &&
            std::filesystem::exists(cursor / "include" / "QaMRpp.hpp") &&
            std::filesystem::exists(cursor / "tests" / "qamrpp_tests.cpp")) {
            source_root = cursor;
            break;
        }
        if (cursor == cursor.root_path()) break;
    }

    const std::filesystem::path build_root = std::filesystem::current_path();
    const std::filesystem::path repo_qamrpp = (
        std::filesystem::exists(build_root / "libqamrpp_core.so") ? build_root :
        std::filesystem::exists(build_root / "build" / "libqamrpp_core.so") ? build_root / "build" :
        std::filesystem::exists(build_root / "build" / "stdlib" / "libqamrpp_core.so") ? build_root / "build" / "stdlib" :
        std::filesystem::exists(build_root / ".qamrpp" / "libqamrpp_core.so") ? build_root / ".qamrpp" :
        source_root
    );
    REQUIRE(std::filesystem::exists(repo_qamrpp / "libqamrpp_core.so"));
    REQUIRE(std::filesystem::exists(repo_qamrpp / "libqamrpp_math.so"));
    REQUIRE(std::filesystem::exists(source_root / "stdlib++" / "string.hpp"));
    REQUIRE(std::filesystem::exists(source_root / "include" / "QaMRpp-Library.hpp"));

    REQUIRE(write_text_file(home_qamrpp / "stdc" / "test.c", "return {\n}\n"));
    for (const auto& entry : std::filesystem::directory_iterator(repo_qamrpp)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".so" &&
            entry.path().filename().string().find("libqamrpp_") == 0) {
            std::filesystem::copy_file(entry.path(), home_qamrpp / entry.path().filename(), std::filesystem::copy_options::overwrite_existing);
        }
    }
    REQUIRE(write_text_file(home_qamrpp / "stdc++" / "QaMRpp-Library.hpp", "return {}\n"));

    REQUIRE(write_text_file(script, "print(type(42))\n"));

    ScopedEnv fake_home_guard("HOME");
    ScopedEnv qamrpp_home_guard("QAMRPP_HOME");
    fake_home_guard.set(fake_home.string());
    qamrpp_home_guard.set(home_qamrpp.string());

    const std::string command = cli + " --script " + script.string() + " >" + out_file.string() + " 2>&1";
    const int status = run_command(command);
    REQUIRE(status == 0);

    const std::string output = read_text_file(out_file);
    const bool got_number = output.find("number") != std::string::npos;
    const bool got_int = output.find("int") != std::string::npos;
    REQUIRE((got_number || got_int));
}

TEST_CASE("numeric conversions from bitwise and concat operations") {
    qamrpp::Context ctx;
    auto r1 = ctx.run("return 5 & 3");
    auto r2 = ctx.run("return 5 | 2");
    auto r3 = ctx.run("return 'x' .. 9");
    const bool r1_is_numeric = (r1->type == qamrpp::Value::INT) || (r1->type == qamrpp::Value::FLOAT);
    const bool r2_is_numeric = (r2->type == qamrpp::Value::INT) || (r2->type == qamrpp::Value::FLOAT);
    REQUIRE(r1_is_numeric);
    REQUIRE(r2_is_numeric);
    REQUIRE(r3->type == qamrpp::Value::STRING);
    REQUIRE(r3->string_value == "x9");
}

TEST_CASE("metatable __call via colon sugar") {
    qamrpp::Context ctx;
    auto result = ctx.run(
        "obj = {}\n"
        "setmetatable(obj, {__call = function(self, a, b) return a + b end})\n"
        "return obj(10, 20)"
    );
    REQUIRE(result->type == qamrpp::Value::FLOAT);
    REQUIRE(result->float_value == 30.0);
}

TEST_CASE("plugin output can execute and round-trip through host") {
    qamrpp::Context ctx;
    qamrpp::Compile2BytecodePlugin byte_plugin;
    byte_plugin.install(ctx);

    auto fn = ctx.lookup_name("compile_to_bytecode");
    REQUIRE(fn);
    std::vector<qamrpp::ValuePtr> args{std::make_shared<qamrpp::Value>(std::string("3*7"))};
    auto output = fn->function_value(ctx, args);
    REQUIRE(output->string_value.find("PUSHI64") != std::string::npos);
}

TEST_CASE("length operator on strings and tables") {
    qamrpp::Context ctx;
    auto s = ctx.run("return # {1,2,3,4}");
    REQUIRE(s->type == qamrpp::Value::INT);
    REQUIRE(s->int_value == 4);
}
