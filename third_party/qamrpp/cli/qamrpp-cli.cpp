#include <glob.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/QaMRpp.hpp"
#include "../plugins/QaMRpp-Compile2C.hpp"
#include "../plugins/QaMRpp-Compile2WASM.hpp"
#include "../plugins/QaMRpp-Dump.hpp"
#include "../plugins/QaMRpp-QBF.hpp"
#include "../plugins/QaMRpp-Readline.hpp"
#include "../plugins/QaMRpp-Serialize2JSON.hpp"
#include "../third_party/Klyspec/include/Klyspec-Manifest.hpp"
#include "../third_party/PikoRL/include/PikoRL.hpp"

namespace {

// Installed periphery directory baked in at build time ($QAMRPP_HOME/cli).
// The CLI tools rely on the files installed at this exact path.
static std::string periphery_file(const char* name) {
#ifdef QAMRPP_CLI_PERIPHERY_DIR
    return (std::filesystem::path(QAMRPP_CLI_PERIPHERY_DIR) / name).string();
#else
    const char* home = std::getenv("QAMRPP_HOME");
    if (home && *home) {
        return (std::filesystem::path(home) / "cli" / name).string();
    }
    return (std::filesystem::path("cli") / name).string();
#endif
}

struct CLIOptions {
    std::vector<std::string> scripts;
    std::vector<std::string> required_files;
    std::vector<std::string> libraries;
    std::string install_library;
    std::string remove_library;
    std::string install_podlet;
    std::string remove_podlet;
    std::string install_name;
    std::string install_root;
    std::vector<std::string> qbf_bundles;
    std::string dump_file;
    std::string config_path;
    std::string compile_out;
    std::string directives_file;
    bool serialize = false;
    bool compile_c = false;
    bool compile_wasm = false;
    bool no_autocomplete = false;
    bool no_color = false;
    bool show_help = false;
    bool show_version = false;
    bool saw_output_file = false;
    bool saw_compile_out = false;
};

struct ReplConfig {
    std::string prompt = "qamrpp> ";
    std::string history_file;
    std::string initial_commands;
    std::string syntax_file = periphery_file("lua.syntax");
    bool autocomplete = true;
    bool color = true;
    bool directives = true;
    std::vector<std::string> standard_libraries;
    std::vector<std::string> excluded_standard_libraries;
};

struct ReplState {
    qamrpp::Context* ctx = nullptr;
    qamrpp::Readline* readline = nullptr;
    ReplConfig config;
    std::vector<std::string> loaded_scripts;
    std::vector<std::string> loaded_libraries;
    std::vector<std::string> directive_history;
    std::vector<std::string> qbf_sources;
    bool running = true;
};

static const char* kCliManifestPath = "cli/QaMRpp-CLI.yaml";
static const char* kLuaFragPath = "cli/lua.frag";

static std::vector<std::string> expand_glob(const std::string& pat) {
    glob_t g;
    std::vector<std::string> out;
    if (glob(pat.c_str(), 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) {
            out.push_back(g.gl_pathv[i]);
        }
    }
    globfree(&g);
    return out;
}

static std::string trim_copy(const std::string& text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

static std::string to_lower_copy(std::string text) {
    for (size_t i = 0; i < text.size(); ++i) {
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    }
    return text;
}

static std::vector<std::string> split_words(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string word;
    while (in >> word) out.push_back(word);
    return out;
}

static std::string join_strings(const std::vector<std::string>& items, size_t start = 0) {
    std::string out;
    for (size_t i = start; i < items.size(); ++i) {
        if (!out.empty()) out += " ";
        out += items[i];
    }
    return out;
}

static std::string read_text_file(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in.good()) return std::string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static std::unordered_map<std::string, std::string> parse_key_value_config(const std::string& path) {
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream in(path.c_str());
    if (!in.good()) return cfg;
    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        cfg[trim_copy(line.substr(0, eq))] = trim_copy(line.substr(eq + 1));
    }
    return cfg;
}

static std::string cfg_get(
    const std::unordered_map<std::string, std::string>& cfg,
    const std::string& key,
    const std::string& fallback
) {
    std::unordered_map<std::string, std::string>::const_iterator it = cfg.find(key);
    return it == cfg.end() ? fallback : it->second;
}

static bool cfg_get_bool(
    const std::unordered_map<std::string, std::string>& cfg,
    const std::string& key,
    bool fallback
) {
    std::unordered_map<std::string, std::string>::const_iterator it = cfg.find(key);
    if (it == cfg.end()) return fallback;
    const std::string value = to_lower_copy(trim_copy(it->second));
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

static qamrpp::ValuePtr call_native(qamrpp::Context& ctx, const std::string& name, std::vector<qamrpp::ValuePtr> args) {
    qamrpp::ValuePtr fn = ctx.lookup_name(name);
    if (!fn || fn->type != qamrpp::Value::FUNCTION) {
        throw std::runtime_error("native function not available: " + name);
    }
    return fn->function_value(ctx, args);
}

static std::string first_word(const std::string& text) {
    size_t end = 0;
    while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
    return text.substr(0, end);
}

static bool starts_with_word(const std::string& text, const std::string& word) {
    if (text.size() < word.size() || text.compare(0, word.size(), word) != 0) return false;
    return text.size() == word.size() || !std::isalnum(static_cast<unsigned char>(text[word.size()]));
}

static int repl_indent_level(const std::string& source) {
    int depth = 0;
    char string_quote = '\0';
    for (size_t i = 0; i < source.size();) {
        if (!string_quote && source[i] == '-' && i + 1 < source.size() && source[i + 1] == '-') {
            while (i < source.size() && source[i] != '\n') ++i;
            continue;
        }
        if ((source[i] == '"' || source[i] == '\'') && (i == 0 || source[i - 1] != '\\')) {
            string_quote = string_quote == source[i] ? '\0' : (string_quote ? string_quote : source[i]);
            ++i;
            continue;
        }
        if (string_quote) {
            ++i;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(source[i])) || source[i] == '_') {
            size_t start = i;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            const std::string word = source.substr(start, i - start);
            if (word == "function" || word == "then" || word == "do" || word == "repeat") ++depth;
            else if ((word == "end" || word == "until") && depth > 0) --depth;
            continue;
        }
        ++i;
    }
    return depth;
}

static bool repl_needs_more_input(const std::string& source) {
    const std::string trimmed = trim_copy(source);
    if (trimmed.empty()) return false;
    if (repl_indent_level(source) > 0) return true;
    const std::string tail = trim_copy(trimmed.substr(trimmed.find_last_of('\n') == std::string::npos ? 0 : trimmed.find_last_of('\n') + 1));
    if (tail.empty()) return true;
    if (tail.back() == ',' || tail.back() == '=' || tail.back() == '(' || tail.back() == '[' || tail.back() == '{' ||
        tail.back() == '+' || tail.back() == '-' || tail.back() == '*' || tail.back() == '/' || tail.back() == '%' ||
        tail.back() == '^' || tail.back() == '&' || tail.back() == '|' || tail.back() == '~' || tail.back() == '#') {
        return true;
    }
    if (tail.size() >= 2) {
        const std::string op = tail.substr(tail.size() - 2);
        if (op == "//" || op == ".." || op == "<<" || op == ">>" || op == "==" || op == "~=" || op == "<=" || op == ">=") {
            return true;
        }
    }
    const std::string word = first_word(tail);
    return word == "function" || word == "if" || word == "while" || word == "for" || word == "repeat";
}

static std::string repl_initial_indent(const std::string& source) {
    int depth = repl_indent_level(source);
    const std::string last = trim_copy(source.substr(source.find_last_of('\n') == std::string::npos ? 0 : source.find_last_of('\n') + 1));
    if ((starts_with_word(last, "end") || starts_with_word(last, "until")) && depth > 0) --depth;
    return std::string(static_cast<size_t>(depth) * 2, ' ');
}

static bool run_report(qamrpp::Context& ctx, const std::string& source, const std::string& label) {
    try {
        (void)ctx.run(source);
        return true;
    } catch (const std::exception& e) {
        std::cerr << label << ": " << e.what() << "\n";
        return false;
    }
}

static std::vector<std::string> split_config_list(const std::string& value) {
    std::vector<std::string> result;
    std::string item;
    for (size_t i = 0; i <= value.size(); ++i) {
        if (i == value.size() || value[i] == ',' || value[i] == ';' || std::isspace(static_cast<unsigned char>(value[i]))) {
            item = trim_copy(item);
            if (!item.empty()) result.push_back(item);
            item.clear();
        } else {
            item.push_back(value[i]);
        }
    }
    return result;
}

static qamrpp::StdLib standard_library_flags(const ReplConfig& config) {
    qamrpp::StdLib flags = qamrpp::StdLib::NONE;
    const std::vector<std::string> all = {"core", "string", "table", "math", "io", "os", "debug", "coroutine", "package", "utf8"};
    const std::vector<std::string>& selected = config.standard_libraries.empty() ? all : config.standard_libraries;
    for (size_t i = 0; i < selected.size(); ++i) {
        if (std::find(config.excluded_standard_libraries.begin(), config.excluded_standard_libraries.end(), selected[i]) != config.excluded_standard_libraries.end()) continue;
        if (selected[i] == "core") flags = flags | qamrpp::StdLib::CORE;
        else if (selected[i] == "string") flags = flags | qamrpp::StdLib::STRING;
        else if (selected[i] == "table") flags = flags | qamrpp::StdLib::TABLE;
        else if (selected[i] == "math") flags = flags | qamrpp::StdLib::MATH;
        else if (selected[i] == "io") flags = flags | qamrpp::StdLib::IO;
        else if (selected[i] == "os") flags = flags | qamrpp::StdLib::OS;
        else if (selected[i] == "debug") flags = flags | qamrpp::StdLib::DEBUGLIB;
        else if (selected[i] == "coroutine") flags = flags | qamrpp::StdLib::COROUTINE;
        else if (selected[i] == "package") flags = flags | qamrpp::StdLib::PACKAGE;
        else if (selected[i] == "utf8") flags = flags | qamrpp::StdLib::UTF8;
    }
    return flags;
}

static void auto_load_user_stdlib(qamrpp::Context& ctx, const ReplConfig& config) {
    ctx.load_standard_library(standard_library_flags(config));
    (void)ctx.load_stdc("");
    (void)ctx.load_stdcpp("");
}

static klyspec::CLIManifestSpec load_cli_manifest_or_throw(const char* argv0) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path(periphery_file("QaMRpp-CLI.yaml")),
        std::filesystem::path(kCliManifestPath),
        std::filesystem::path("../") / kCliManifestPath,
        std::filesystem::path("../../") / kCliManifestPath
    };

    if (argv0 && *argv0) {
        const std::filesystem::path exe = std::filesystem::absolute(argv0);
        const std::filesystem::path exe_dir = exe.parent_path();
        candidates.push_back(exe_dir / kCliManifestPath);
        candidates.push_back(exe_dir / "../" / kCliManifestPath);
        candidates.push_back(exe_dir / "../../" / kCliManifestPath);
    }

    std::vector<std::string> diagnostics;
    for (const std::filesystem::path& candidate : candidates) {
        const klyspec::CLIManifestResult result = klyspec::load_cli_manifest_file(candidate.string());
        if (result.ok && result.manifest.has_value()) {
            return *result.manifest;
        }
        diagnostics.insert(diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
    }

    std::ostringstream error;
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        if (i) error << "; ";
        error << diagnostics[i];
    }
    throw std::runtime_error(error.str().empty() ? "failed to load CLI manifest" : error.str());
}

static klyspec::Registry build_registry(const klyspec::CLIManifestSpec& manifest) {
    klyspec::Registry registry;
    klyspec::CommandSpec command;
    command.name = manifest.program;
    command.help = manifest.about;
    if (!registry.register_command(command)) {
        throw std::runtime_error("failed to register command from CLI manifest");
    }
    for (size_t i = 0; i < manifest.arguments.size(); ++i) {
        klyspec::ArgumentSpec arg;
        arg.id = manifest.arguments[i].id;
        arg.names = manifest.arguments[i].names;
        arg.help = manifest.arguments[i].help;
        arg.required = manifest.arguments[i].required;
        arg.default_value = manifest.arguments[i].default_value;
        arg.kind = manifest.arguments[i].kind == "flag" ? klyspec::ArgumentKind::flag :
                   manifest.arguments[i].kind == "positional" ? klyspec::ArgumentKind::positional :
                   manifest.arguments[i].kind == "variadic" ? klyspec::ArgumentKind::variadic :
                   klyspec::ArgumentKind::option;
        arg.value_policy = manifest.arguments[i].kind == "flag" ? klyspec::ValuePolicy::none : klyspec::ValuePolicy::required;
        if (!registry.register_argument(command.name, arg)) {
            throw std::runtime_error("failed to register CLI argument: " + arg.id);
        }
    }
    return registry;
}

static void print_help_from_manifest(const klyspec::CLIManifestSpec& manifest) {
    std::cout << klyspec::generate_help_page(manifest);
}

static std::vector<std::string> get_values(
    const klyspec::ParseResult& result,
    const std::string& key
) {
    std::unordered_map<std::string, std::vector<std::string>>::const_iterator it = result.values.find(key);
    return it == result.values.end() ? std::vector<std::string>() : it->second;
}

static bool has_flag(const klyspec::ParseResult& result, const std::string& key) {
    return result.values.find(key) != result.values.end();
}

static CLIOptions parse_cli_options(const klyspec::CLIManifestSpec& manifest, int argc, char** argv) {
    klyspec::Registry registry = build_registry(manifest);
    klyspec::KlyCLIService cli(registry);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
    const klyspec::ParseResult parsed = cli.parse(manifest.program, args);
    if (!parsed.ok) {
        std::ostringstream error;
        for (size_t i = 0; i < parsed.diagnostics.size(); ++i) {
            if (i) error << "; ";
            error << parsed.diagnostics[i];
        }
        throw std::runtime_error(error.str());
    }

    CLIOptions options;
    options.show_help = has_flag(parsed, "help");
    options.show_version = has_flag(parsed, "version");
    options.scripts = get_values(parsed, "script");
    options.required_files = get_values(parsed, "require");
    options.libraries = get_values(parsed, "load");
    {
        std::vector<std::string> values = get_values(parsed, "install_library");
        if (!values.empty()) options.install_library = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "remove_library");
        if (!values.empty()) options.remove_library = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "install_podlet");
        if (!values.empty()) options.install_podlet = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "remove_podlet");
        if (!values.empty()) options.remove_podlet = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "name");
        if (!values.empty()) options.install_name = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "root");
        if (!values.empty()) options.install_root = values.back();
    }
    options.qbf_bundles = get_values(parsed, "qbf");
    {
        std::vector<std::string> values = get_values(parsed, "dump");
        if (!values.empty()) options.dump_file = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "config");
        if (!values.empty()) options.config_path = values.back();
    }
    {
        std::vector<std::string> values = get_values(parsed, "output_file");
        if (!values.empty()) {
            options.compile_out = values.back();
            options.saw_output_file = true;
        }
    }
    {
        std::vector<std::string> values = get_values(parsed, "compile_out");
        if (!values.empty()) {
            options.compile_out = values.back();
            options.saw_compile_out = true;
        }
    }
    {
        std::vector<std::string> values = get_values(parsed, "directives_file");
        if (!values.empty()) options.directives_file = values.back();
    }
    options.serialize = has_flag(parsed, "serialize");
    options.compile_c = has_flag(parsed, "compile_c");
    options.compile_wasm = has_flag(parsed, "compile_wasm");
    options.no_autocomplete = has_flag(parsed, "no_autocomplete");
    options.no_color = has_flag(parsed, "no_color");
    return options;
}

static std::string bool_word(bool value) {
    return value ? "on" : "off";
}

static std::vector<std::string> base_completion_words() {
    const char* words[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
        "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
        "true", "until", "while", "exit", "quit", "help", "print", "require"
    };
    return std::vector<std::string>(words, words + sizeof(words) / sizeof(words[0]));
}

static std::vector<std::string> directive_words() {
    const char* words[] = {
        "%help", "%autocomplete", "%color", "%history", "%prompt", "%syntax",
        "%load", "%require", "%run", "%source", "%dump", "%serialize", "%compile-c",
        "%compile-wasm", "%pwd", "%cd", "%home", "%libs", "%scripts", "%qbf",
        "%clear", "%reset", "%status", "%quit"
    };
    return std::vector<std::string>(words, words + sizeof(words) / sizeof(words[0]));
}

static std::vector<std::string> collect_completions(const ReplState& state, const std::string& prefix) {
    std::set<std::string> words;
    const std::vector<std::string> base = base_completion_words();
    const std::vector<std::string> directives = directive_words();
    words.insert(base.begin(), base.end());
    words.insert(directives.begin(), directives.end());
    words.insert(state.loaded_scripts.begin(), state.loaded_scripts.end());
    words.insert(state.loaded_libraries.begin(), state.loaded_libraries.end());
    for (std::map<std::string, qamrpp::ValuePtr>::const_iterator it = state.ctx->globals.begin(); it != state.ctx->globals.end(); ++it) {
        words.insert(it->first);
    }

    std::vector<std::string> out;
    for (std::set<std::string>::const_iterator it = words.begin(); it != words.end(); ++it) {
        if (it->find(prefix) == 0) out.push_back(*it);
    }
    return out;
}

static void load_lua_frag(qamrpp::Context& ctx) {
    std::string frag = read_text_file(periphery_file("lua.frag"));
    if (frag.empty()) {
        frag = read_text_file(kLuaFragPath);
    }
    if (!frag.empty()) {
        (void)run_report(ctx, frag, "lua.frag");
    }
}

static ReplConfig load_repl_config(const CLIOptions& options) {
    ReplConfig config;
    if (!options.config_path.empty()) {
        const std::unordered_map<std::string, std::string> cfg = parse_key_value_config(options.config_path);
        config.prompt = cfg_get(cfg, "prompt", config.prompt);
        config.history_file = cfg_get(cfg, "history_file", config.history_file);
        config.initial_commands = cfg_get(cfg, "initial_commands", config.initial_commands);
        config.syntax_file = cfg_get(cfg, "syntax_file", config.syntax_file);
        config.autocomplete = cfg_get_bool(cfg, "autocomplete", config.autocomplete);
        config.color = cfg_get_bool(cfg, "color", config.color);
        config.directives = cfg_get_bool(cfg, "directives", config.directives);
        config.standard_libraries = split_config_list(cfg_get(cfg, "standard_libraries", ""));
        config.excluded_standard_libraries = split_config_list(cfg_get(cfg, "exclude_standard_libraries", ""));
    }
    if (options.no_autocomplete) config.autocomplete = false;
    if (options.no_color) config.color = false;
    return config;
}

static bool apply_script_file(ReplState& state, const std::string& path, bool required, const std::string& label) {
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in.good()) {
        if (required) std::cerr << label << ": cannot open script: " << path << "\n";
        return false;
    }
    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!run_report(*state.ctx, source, label)) return false;
    state.loaded_scripts.push_back(path);
    return true;
}

static bool apply_library(ReplState& state, const std::string& path_or_name) {
    if (!state.ctx->load_library(path_or_name) && !state.ctx->load_library_named(path_or_name)) {
        std::cerr << "directive: failed to load library: " << path_or_name << "\n";
        return false;
    }
    state.loaded_libraries.push_back(path_or_name);
    return true;
}

static bool has_management_action(const CLIOptions& options) {
    return !options.install_library.empty() ||
           !options.remove_library.empty() ||
           !options.install_podlet.empty() ||
           !options.remove_podlet.empty() ||
           !options.install_name.empty() ||
           !options.install_root.empty();
}

static void validate_management_mode(const CLIOptions& options) {
    if (!has_management_action(options)) {
        return;
    }
    if (!options.scripts.empty() ||
        !options.required_files.empty() ||
        !options.libraries.empty() ||
        !options.qbf_bundles.empty() ||
        !options.dump_file.empty() ||
        !options.config_path.empty() ||
        !options.compile_out.empty() ||
        options.serialize ||
        options.compile_c ||
        options.compile_wasm) {
        throw std::runtime_error("library/podlet management options cannot be combined with execution options");
    }
    const int action_count =
        (!options.install_library.empty() ? 1 : 0) +
        (!options.remove_library.empty() ? 1 : 0) +
        (!options.install_podlet.empty() ? 1 : 0) +
        (!options.remove_podlet.empty() ? 1 : 0);
    if (action_count != 1) {
        throw std::runtime_error("choose exactly one of --install-library, --remove-library, --install-podlet, or --remove-podlet");
    }
}

static int run_management_mode(const CLIOptions& options) {
    qamrpp::Context ctx;
    const std::string& root = options.install_root;
    if (!options.install_library.empty()) {
        if (!ctx.install_library(options.install_library, options.install_name, root)) {
            std::cerr << "qamrpp-cli: " << ctx.last_error_message << "\n";
            return 1;
        }
        std::cout << "installed library: " << options.install_library << "\n";
        return 0;
    }
    if (!options.remove_library.empty()) {
        if (!ctx.remove_library(options.remove_library, root)) {
            std::cerr << "qamrpp-cli: " << ctx.last_error_message << "\n";
            return 1;
        }
        std::cout << "removed library: " << options.remove_library << "\n";
        return 0;
    }
    if (!options.install_podlet.empty()) {
        if (!ctx.install_podlet(options.install_podlet, options.install_name, root)) {
            std::cerr << "qamrpp-cli: " << ctx.last_error_message << "\n";
            return 1;
        }
        std::cout << "installed podlet: " << options.install_podlet << "\n";
        return 0;
    }
    if (!options.remove_podlet.empty()) {
        if (!ctx.remove_podlet(options.remove_podlet, root)) {
            std::cerr << "qamrpp-cli: " << ctx.last_error_message << "\n";
            return 1;
        }
        std::cout << "removed podlet: " << options.remove_podlet << "\n";
        return 0;
    }
    return 0;
}

static bool write_text_file(const std::string& path, const std::string& text) {
    std::ofstream out(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.good()) return false;
    out << text;
    return out.good();
}

static std::string linked_source_from_files(const std::vector<std::string>& files, const std::vector<std::string>& qbf_sources) {
    std::string linked_source;
    for (size_t i = 0; i < qbf_sources.size(); ++i) {
        linked_source += qbf_sources[i];
        if (!qbf_sources[i].empty() && qbf_sources[i].back() != '\n') linked_source.push_back('\n');
    }
    for (size_t i = 0; i < files.size(); ++i) {
        std::ifstream in(files[i].c_str(), std::ios::in | std::ios::binary);
        if (!in.good()) throw std::runtime_error("cannot open script: " + files[i]);
        const std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        linked_source += src;
        if (!src.empty() && src.back() != '\n') linked_source.push_back('\n');
    }
    return linked_source;
}

static bool compile_and_emit(
    qamrpp::Context& ctx,
    const std::vector<std::string>& files,
    const std::vector<std::string>& qbf_sources,
    bool compile_c,
    const std::string& output_path
) {
    std::vector<qamrpp::ValuePtr> args;
    args.push_back(std::make_shared<qamrpp::Value>(linked_source_from_files(files, qbf_sources)));
    qamrpp::ValuePtr compiled = compile_c ? call_native(ctx, "compile_to_c", args) : call_native(ctx, "compile_to_wasm", args);
    const std::string out_text = compiled ? compiled->to_string() : std::string();
    if (output_path.empty()) {
        std::cout << out_text << "\n";
        return true;
    }
    return write_text_file(output_path, out_text);
}

static void print_repl_help() {
    std::cout
        << "REPL directives:\n"
        << "  %help                         Show directive help\n"
        << "  %autocomplete on|off         Toggle autocomplete\n"
        << "  %color on|off                Toggle syntax colors\n"
        << "  %history save [file]         Save history\n"
        << "  %history load [file]         Load history\n"
        << "  %history clear               Clear history file state\n"
        << "  %prompt <text>               Change prompt\n"
        << "  %syntax reload [file]        Reload syntax definition\n"
        << "  %load <lib>                  Load plugin/library\n"
        << "  %require <file>              Run a required script\n"
        << "  %run <file>                  Run a script file\n"
        << "  %source <file>               Alias of %run\n"
        << "  %dump <file.dot>             Set dump output file\n"
        << "  %serialize                   Print serialized program snapshot\n"
        << "  %compile-c [file]            Emit C translation\n"
        << "  %compile-wasm [file]         Emit WAT translation\n"
        << "  %pwd                         Print current directory\n"
        << "  %cd <dir>                    Change current directory\n"
        << "  %home                        Print QaMRpp home root\n"
        << "  %libs                        List loaded libraries\n"
        << "  %scripts                     List executed scripts\n"
        << "  %qbf <file>                  Load QBF bundle\n"
        << "  %clear                       Clear the screen\n"
        << "  %reset                       Reset prompt/colors/autocomplete\n"
        << "  %status                      Print REPL status\n"
        << "  %quit                        Exit the REPL\n";
}

static bool is_truthy_word(const std::string& word) {
    const std::string lower = to_lower_copy(word);
    return lower == "on" || lower == "true" || lower == "yes" || lower == "1";
}

static bool run_directive(ReplState& state, const std::string& line) {
    const std::vector<std::string> words = split_words(line);
    if (words.empty()) return true;
    const std::string cmd = words[0];
    state.directive_history.push_back(line);

    if (cmd == "%help") {
        print_repl_help();
        return true;
    }
    if (cmd == "%quit") {
        state.running = false;
        return true;
    }
    if (cmd == "%autocomplete") {
        if (words.size() < 2) {
            std::cout << "autocomplete " << bool_word(state.config.autocomplete) << "\n";
            return true;
        }
        state.config.autocomplete = is_truthy_word(words[1]);
        std::cout << "autocomplete " << bool_word(state.config.autocomplete) << "\n";
        return true;
    }
    if (cmd == "%color") {
        if (words.size() < 2) {
            std::cout << "color " << bool_word(state.config.color) << "\n";
            return true;
        }
        state.config.color = is_truthy_word(words[1]);
        state.readline->set_color(state.config.color);
        std::cout << "color " << bool_word(state.config.color) << "\n";
        return true;
    }
    if (cmd == "%prompt") {
        if (words.size() < 2) {
            std::cout << state.config.prompt << "\n";
            return true;
        }
        state.config.prompt = join_strings(words, 1);
        return true;
    }
    if (cmd == "%syntax") {
        const std::string path = words.size() > 2 ? join_strings(words, 2) : (words.size() > 1 && words[1] != "reload" ? join_strings(words, 1) : state.config.syntax_file);
        if (!path.empty()) {
            state.config.syntax_file = path;
            const bool ok = state.readline->load_syntax(path);
            std::cout << (ok ? "syntax reloaded" : "syntax reload failed") << ": " << path << "\n";
        }
        return true;
    }
    if (cmd == "%history") {
        if (words.size() < 2) {
            std::cout << (state.config.history_file.empty() ? "(history disabled)" : state.config.history_file) << "\n";
            return true;
        }
        if (words[1] == "clear") {
            state.config.history_file.clear();
            std::cout << "history disabled\n";
            return true;
        }
        const std::string path = words.size() > 2 ? join_strings(words, 2) : state.config.history_file;
        if (path.empty()) {
            std::cerr << "directive: history file not set\n";
            return true;
        }
        if (words[1] == "save") {
            state.readline->save_history(path);
            state.config.history_file = path;
            std::cout << "history saved: " << path << "\n";
            return true;
        }
        if (words[1] == "load") {
            state.readline->load_history(path);
            state.config.history_file = path;
            std::cout << "history loaded: " << path << "\n";
            return true;
        }
        std::cerr << "directive: unknown history subcommand\n";
        return true;
    }
    if (cmd == "%load") {
        if (words.size() < 2) {
            std::cerr << "directive: %load expects a library name or path\n";
            return true;
        }
        (void)apply_library(state, join_strings(words, 1));
        return true;
    }
    if (cmd == "%require") {
        if (words.size() < 2) {
            std::cerr << "directive: %require expects a file\n";
            return true;
        }
        (void)apply_script_file(state, join_strings(words, 1), true, "directive");
        return true;
    }
    if (cmd == "%run" || cmd == "%source") {
        if (words.size() < 2) {
            std::cerr << "directive: " << cmd << " expects a file\n";
            return true;
        }
        (void)apply_script_file(state, join_strings(words, 1), true, "directive");
        return true;
    }
    if (cmd == "%dump") {
        if (words.size() < 2) {
            std::cerr << "directive: %dump expects an output path\n";
            return true;
        }
        state.ctx->assign_name("__qamrpp_dump_file", std::make_shared<qamrpp::Value>(join_strings(words, 1)));
        std::vector<qamrpp::ValuePtr> args;
        args.push_back(std::make_shared<qamrpp::Value>(join_strings(words, 1)));
        (void)call_native(*state.ctx, "set_dump_file", args);
        std::cout << "dump file set\n";
        return true;
    }
    if (cmd == "%serialize") {
        std::vector<qamrpp::ValuePtr> noargs;
        qamrpp::ValuePtr v = call_native(*state.ctx, "get_serialized_program", noargs);
        std::cout << (v ? v->to_string() : std::string()) << "\n";
        return true;
    }
    if (cmd == "%compile-c" || cmd == "%compile-wasm") {
        const bool ok = compile_and_emit(*state.ctx, state.loaded_scripts, state.qbf_sources, cmd == "%compile-c", words.size() > 1 ? join_strings(words, 1) : std::string());
        if (!ok) std::cerr << "directive: failed to write compilation output\n";
        return true;
    }
    if (cmd == "%pwd") {
        std::cout << std::filesystem::current_path().string() << "\n";
        return true;
    }
    if (cmd == "%cd") {
        if (words.size() < 2) {
            std::cerr << "directive: %cd expects a path\n";
            return true;
        }
        std::error_code ec;
        std::filesystem::current_path(join_strings(words, 1), ec);
        if (ec) std::cerr << "directive: " << ec.message() << "\n";
        return true;
    }
    if (cmd == "%home") {
        std::cout << qamrpp::Context::home_root() << "\n";
        return true;
    }
    if (cmd == "%libs") {
        for (size_t i = 0; i < state.loaded_libraries.size(); ++i) std::cout << state.loaded_libraries[i] << "\n";
        return true;
    }
    if (cmd == "%scripts") {
        for (size_t i = 0; i < state.loaded_scripts.size(); ++i) std::cout << state.loaded_scripts[i] << "\n";
        return true;
    }
    if (cmd == "%qbf") {
        if (words.size() < 2) {
            std::cerr << "directive: %qbf expects a bundle path\n";
            return true;
        }
        std::vector<qamrpp::QBFEntry> entries;
        if (!qamrpp::read_qbf(join_strings(words, 1), entries)) {
            std::cerr << "directive: failed to read qbf\n";
            return true;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            state.ctx->linker.add_source(entries[i].path, entries[i].source);
            state.qbf_sources.push_back(entries[i].source);
        }
        std::cout << "qbf loaded: " << entries.size() << " entries\n";
        return true;
    }
    if (cmd == "%clear") {
        std::cout << "\033[2J\033[H";
        return true;
    }
    if (cmd == "%reset") {
        state.config.prompt = "qamrpp> ";
        state.config.autocomplete = true;
        state.config.color = true;
        state.readline->set_color(true);
        std::cout << "repl reset\n";
        return true;
    }
    if (cmd == "%status") {
        std::cout
            << "prompt=" << state.config.prompt << "\n"
            << "autocomplete=" << bool_word(state.config.autocomplete) << "\n"
            << "color=" << bool_word(state.config.color) << "\n"
            << "history=" << (state.config.history_file.empty() ? "(disabled)" : state.config.history_file) << "\n"
            << "scripts=" << state.loaded_scripts.size() << "\n"
            << "libs=" << state.loaded_libraries.size() << "\n";
        return true;
    }

    std::cerr << "directive: unknown command: " << cmd << "\n";
    return true;
}

static void run_directives_file(ReplState& state, const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in.good()) throw std::runtime_error("cannot open directives file: " + path);
    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') continue;
        if (!line.empty() && line[0] == '%') {
            (void)run_directive(state, line);
        } else {
            (void)run_report(*state.ctx, line, "directive-file");
        }
    }
}

static int run_repl(qamrpp::Context& ctx, const CLIOptions& options) {
    ReplConfig config = load_repl_config(options);
    load_lua_frag(ctx);

    if (!config.initial_commands.empty()) {
        (void)run_report(ctx, config.initial_commands, "initial command error");
    }

    qamrpp::Readline rl;
    rl.set_color(config.color);
    (void)rl.load_syntax(config.syntax_file);

    ReplState state;
    state.ctx = &ctx;
    state.readline = &rl;
    state.config = config;

    if (!config.history_file.empty()) rl.load_history(config.history_file);
    if (!options.directives_file.empty()) run_directives_file(state, options.directives_file);

    rl.set_completer([&state](const std::string& prefix) -> std::vector<std::string> {
        if (!state.config.autocomplete) return std::vector<std::string>();
        return collect_completions(state, prefix);
    });

    while (state.running) {
        std::string source;
        while (state.running) {
            const std::string current_prompt = source.empty() ? state.config.prompt : std::string("... ");
            const std::string initial = source.empty() ? std::string() : repl_initial_indent(source);
            std::string line;
            const bool got_line = initial.empty() ? rl.read_line(current_prompt, line) : rl.read_line(current_prompt, line, initial);
            if (!got_line) {
                if (!state.config.history_file.empty()) rl.save_history(state.config.history_file);
                return 0;
            }

            const std::string command = trim_copy(line);
            if (source.empty() && (command == "exit" || command == "quit")) {
                if (!state.config.history_file.empty()) rl.save_history(state.config.history_file);
                return 0;
            }
            if (source.empty() && command.empty()) break;
            if (source.empty() && state.config.directives && !command.empty() && command[0] == '%') {
                (void)run_directive(state, command);
                line.clear();
                break;
            }

            if (!source.empty()) source.push_back('\n');
            source += line;
            if (!repl_needs_more_input(source)) break;
        }
        if (trim_copy(source).empty()) continue;
        rl.add_history(source);
        (void)run_report(ctx, source, "error");
    }

    if (!state.config.history_file.empty()) rl.save_history(state.config.history_file);
    return 0;
}

static void validate_cli_mode(const CLIOptions& options, qamrpp::Context& ctx) {
    if (has_management_action(options)) {
        return;
    }
    if (options.compile_c && options.compile_wasm) {
        throw std::runtime_error("choose either --compile-c or --compile-wasm");
    }
    if ((options.saw_output_file || options.saw_compile_out) && !options.compile_c && !options.compile_wasm) {
        throw std::runtime_error("--output-file/--compile-out requires --compile-c or --compile-wasm");
    }
    if (options.saw_output_file && options.saw_compile_out) {
        throw std::runtime_error("use one of --output-file or --compile-out, not both");
    }
    if ((options.compile_c || options.compile_wasm) && options.serialize) {
        throw std::runtime_error("--serialize conflicts with --compile-c/--compile-wasm");
    }
    if ((options.compile_c || options.compile_wasm) && !options.dump_file.empty()) {
        throw std::runtime_error("--dump conflicts with --compile-c/--compile-wasm");
    }
    if (!options.config_path.empty() && (!options.scripts.empty() || ctx.linker.size() > 0 || options.compile_c || options.compile_wasm)) {
        throw std::runtime_error("--config is only valid for REPL mode");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const klyspec::CLIManifestSpec manifest = load_cli_manifest_or_throw(argv[0]);
        const CLIOptions options = parse_cli_options(manifest, argc, argv);

        if (options.show_help) {
            print_help_from_manifest(manifest);
            return 0;
        }
        if (options.show_version) {
            std::cout << manifest.program << " " << manifest.version << "\n";
            return 0;
        }
        validate_management_mode(options);
        if (has_management_action(options)) {
            return run_management_mode(options);
        }

        ReplConfig config = load_repl_config(options);
        qamrpp::Context ctx;
        auto_load_user_stdlib(ctx, config);
        load_lua_frag(ctx);

        picorl::PicoRLExtension picorl_extension;
        picorl_extension.register_functions(ctx);

        qamrpp::DumpPlugin dump_plugin;
        qamrpp::Serialize2JSONPlugin serialize_plugin;
        qamrpp::Compile2CPlugin compile2c_plugin;
        qamrpp::Compile2WASMPlugin compile2wasm_plugin;
        dump_plugin.install(ctx);
        serialize_plugin.install(ctx);
        compile2c_plugin.install(ctx);
        compile2wasm_plugin.install(ctx);

        std::vector<std::string> scripts;
        for (size_t i = 0; i < options.scripts.size(); ++i) {
            std::vector<std::string> expanded = expand_glob(options.scripts[i]);
            scripts.insert(scripts.end(), expanded.begin(), expanded.end());
        }

        if (!options.dump_file.empty()) {
            std::vector<qamrpp::ValuePtr> args;
            args.push_back(std::make_shared<qamrpp::Value>(options.dump_file));
            (void)call_native(ctx, "set_dump_file", args);
        }

        for (size_t i = 0; i < options.libraries.size(); ++i) {
            if (!ctx.load_library(options.libraries[i]) && !ctx.load_library_named(options.libraries[i])) {
                throw std::runtime_error("failed to load library: " + options.libraries[i]);
            }
        }
        for (size_t i = 0; i < options.required_files.size(); ++i) {
            if (!ctx.linker.add_file(options.required_files[i])) {
                throw std::runtime_error("cannot open required file: " + options.required_files[i]);
            }
        }

        std::vector<std::string> qbf_sources;
        for (size_t i = 0; i < options.qbf_bundles.size(); ++i) {
            std::vector<qamrpp::QBFEntry> entries;
            if (!qamrpp::read_qbf(options.qbf_bundles[i], entries)) {
                throw std::runtime_error("failed to read qbf: " + options.qbf_bundles[i]);
            }
            for (size_t j = 0; j < entries.size(); ++j) {
                ctx.linker.add_source(entries[j].path, entries[j].source);
                qbf_sources.push_back(entries[j].source);
            }
        }

        validate_cli_mode(options, ctx);

        if (scripts.empty() && ctx.linker.size() == 0) {
            return run_repl(ctx, options);
        }

        for (size_t i = 0; i < scripts.size(); ++i) {
            if (!ctx.linker.add_file(scripts[i])) throw std::runtime_error("cannot open script: " + scripts[i]);
        }

        std::vector<std::string> compile_units = options.required_files;
        compile_units.insert(compile_units.end(), scripts.begin(), scripts.end());

        if ((options.compile_c || options.compile_wasm) && compile_units.empty() && qbf_sources.empty()) {
            throw std::runtime_error("--compile-c/--compile-wasm requires --script, --require, or --qbf input");
        }

        if (options.compile_c || options.compile_wasm) {
            if (!compile_and_emit(ctx, compile_units, qbf_sources, options.compile_c, options.compile_out)) {
                throw std::runtime_error("cannot write compile output: " + options.compile_out);
            }
            return 0;
        }

        try {
            (void)ctx.linker.link(ctx);
        } catch (const std::exception& e) {
            std::cerr << "qamrpp-cli: " << e.what() << "\n";
            return 1;
        }

        if (options.serialize) {
            std::vector<qamrpp::ValuePtr> noargs;
            qamrpp::ValuePtr v = call_native(ctx, "get_serialized_program", noargs);
            std::cout << (v ? v->to_string() : std::string()) << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "qamrpp-cli: " << e.what() << "\n";
        return 1;
    }
}
