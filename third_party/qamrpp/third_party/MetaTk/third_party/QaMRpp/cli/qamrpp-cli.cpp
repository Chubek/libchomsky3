#include <glob.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cctype>
#include <unordered_map>
#include <vector>

#include "../include/QaMRpp.hpp"
#include "../third_party/Klyspec/Klyspec.hpp"
#include "../plugins/QaMRpp-Compile2C.hpp"
#include "../plugins/QaMRpp-Compile2WASM.hpp"
#include "../plugins/QaMRpp-Dump.hpp"
#include "../plugins/QaMRpp-Readline.hpp"
#include "../plugins/QaMRpp-Serialize2JSON.hpp"
#include "../plugins/QaMRpp-QBF.hpp"

static std::vector<std::string> expand_glob(const std::string& pat) {
    glob_t g;
    std::vector<std::string> out;
    if (glob(pat.c_str(), 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) out.push_back(g.gl_pathv[i]);
    }
    globfree(&g);
    return out;
}

static std::unordered_map<std::string, std::string> klyspec_parse(const std::string& path) {
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream in(path.c_str());
    if (!in) {
        return cfg;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        cfg[key] = value;
    }
    return cfg;
}

static std::string klyspec_get(const std::unordered_map<std::string, std::string>& cfg, const std::string& key, const std::string& fallback) {
    std::unordered_map<std::string, std::string>::const_iterator it = cfg.find(key);
    return it == cfg.end() ? fallback : it->second;
}

static void print_help() {
    std::cout
        << "Usage:\n"
        << "  qamrpp-cli [options]\n\n"
        << "Core options:\n"
        << "  -s, --script <glob>      Add script files by glob pattern\n"
        << "      --require <file>     Add required source unit before scripts\n"
        << "      --load <name|path>   Load native library by name or path\n"
        << "      --qbf <bundle>       Load QBF bundle units into linker\n"
        << "      --config <file.kly>  REPL config file\n\n"
        << "Inspection options:\n"
        << "      --dump <file.dot>    Write AST Graphviz dump\n"
        << "      --serialize          Print JSON serialization snapshot\n\n"
        << "Translation options:\n"
        << "      --compile-c          Translate linked input to C source\n"
        << "      --compile-wasm       Translate linked input to WAT source\n"
        << "      --output-file <path> Write translation output to file\n"
        << "      --compile-out <path> Alias of --output-file (deprecated)\n\n"
        << "Meta options:\n"
        << "  -h, --help               Show this help and exit\n";
}

static qamrpp::ValuePtr call_native(qamrpp::Context& ctx, const std::string& name, std::vector<qamrpp::ValuePtr> args) {
    qamrpp::ValuePtr fn = ctx.lookup_name(name);
    if (!fn || fn->type != qamrpp::Value::FUNCTION) {
        throw std::runtime_error("native function not available: " + name);
    }
    return fn->function_value(ctx, args);
}

static std::string trim_copy(const std::string& text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

static bool starts_with_word(const std::string& text, const std::string& word) {
    if (text.size() < word.size() || text.compare(0, word.size(), word) != 0) return false;
    return text.size() == word.size() || !std::isalnum(static_cast<unsigned char>(text[word.size()]));
}

static std::string first_word(const std::string& text) {
    size_t end = 0;
    while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
    return text.substr(0, end);
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
            std::string word = source.substr(start, i - start);
            if (word == "function" || word == "then" || word == "do" || word == "repeat") {
                ++depth;
            } else if (word == "end" || word == "until") {
                if (depth > 0) --depth;
            }
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
        if (op == "//" || op == ".." || op == "<<" || op == ">>" || op == "==" || op == "~=" ||
            op == "<=" || op == ">=") {
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

static void auto_load_user_stdlib(qamrpp::Context& ctx) {
    ctx.load_standard_library();
    (void)ctx.load_stdc("");
    (void)ctx.load_stdcpp("");
}

int main(int argc, char** argv) {
    try {
    qamrpp::Context ctx;
    auto_load_user_stdlib(ctx);
    qamrpp::DumpPlugin dump_plugin;
    qamrpp::Serialize2JSONPlugin serialize_plugin;
    qamrpp::Compile2CPlugin compile2c_plugin;
    qamrpp::Compile2WASMPlugin compile2wasm_plugin;
    dump_plugin.install(ctx);
    serialize_plugin.install(ctx);
    compile2c_plugin.install(ctx);
    compile2wasm_plugin.install(ctx);

    std::vector<std::string> scripts;
    std::vector<std::string> required_files;
    std::vector<std::string> libraries;
    std::string dump_file;
    bool serialize = false;
    std::string config_path;
    bool compile_c = false;
    bool compile_wasm = false;
    std::string compile_out;
    bool saw_output_file = false;
    bool saw_compile_out = false;
    std::vector<std::string> qbf_sources;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
        if (arg == "--script" || arg == "-s") {
            if (i + 1 >= argc) throw std::runtime_error("--script expects a glob pattern");
            std::vector<std::string> m = expand_glob(argv[++i]);
            scripts.insert(scripts.end(), m.begin(), m.end());
        } else if (arg == "--load") {
            if (i + 1 >= argc) throw std::runtime_error("--load expects library name/path");
            libraries.push_back(argv[++i]);
        } else if (arg == "--require") {
            if (i + 1 >= argc) throw std::runtime_error("--require expects script path");
            required_files.push_back(argv[++i]);
        } else if (arg == "--dump") {
            if (i + 1 >= argc) throw std::runtime_error("--dump expects output file");
            dump_file = argv[++i];
            std::vector<qamrpp::ValuePtr> args;
            args.push_back(std::make_shared<qamrpp::Value>(dump_file));
            (void)call_native(ctx, "set_dump_file", std::move(args));
        } else if (arg == "--serialize") {
            serialize = true;
        } else if (arg == "--compile-c") {
            compile_c = true;
        } else if (arg == "--compile-wasm") {
            compile_wasm = true;
        } else if (arg == "--output-file") {
            if (i + 1 >= argc) throw std::runtime_error("--output-file expects output file");
            compile_out = argv[++i];
            saw_output_file = true;
        } else if (arg == "--compile-out") {
            if (i + 1 >= argc) throw std::runtime_error("--compile-out expects output file");
            compile_out = argv[++i];
            saw_compile_out = true;
        } else if (arg == "--config") {
            if (i + 1 >= argc) throw std::runtime_error("--config expects .kly path");
            config_path = argv[++i];
        } else if (arg == "--qbf") {
            if (i + 1 >= argc) throw std::runtime_error("--qbf expects bundle path");
            std::vector<qamrpp::QBFEntry> entries;
            if (!qamrpp::read_qbf(argv[++i], entries)) throw std::runtime_error("failed to read qbf");
            for (size_t k = 0; k < entries.size(); ++k) {
                ctx.linker.add_source(entries[k].path, entries[k].source);
                qbf_sources.push_back(entries[k].source);
            }
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    for (size_t i = 0; i < libraries.size(); ++i) {
        if (!ctx.load_library(libraries[i]) && !ctx.load_library_named(libraries[i])) {
            throw std::runtime_error("failed to load library: " + libraries[i]);
        }
    }
    for (size_t i = 0; i < required_files.size(); ++i) {
        if (!ctx.linker.add_file(required_files[i])) {
            throw std::runtime_error("cannot open required file: " + required_files[i]);
        }
    }

    std::vector<std::string> compile_units = required_files;
    compile_units.insert(compile_units.end(), scripts.begin(), scripts.end());
    if (compile_c && compile_wasm) {
        throw std::runtime_error("choose either --compile-c or --compile-wasm");
    }
    if ((saw_output_file || saw_compile_out) && !compile_c && !compile_wasm) {
        throw std::runtime_error("--output-file/--compile-out requires --compile-c or --compile-wasm");
    }
    if (saw_output_file && saw_compile_out) {
        throw std::runtime_error("use one of --output-file or --compile-out, not both");
    }
    if ((compile_c || compile_wasm) && serialize) {
        throw std::runtime_error("--serialize conflicts with --compile-c/--compile-wasm");
    }
    if ((compile_c || compile_wasm) && !dump_file.empty()) {
        throw std::runtime_error("--dump conflicts with --compile-c/--compile-wasm");
    }
    if ((compile_c || compile_wasm) && compile_units.empty() && qbf_sources.empty()) {
        throw std::runtime_error("--compile-c/--compile-wasm requires --script, --require, or --qbf input");
    }
    if (!config_path.empty() && (!scripts.empty() || ctx.linker.size() > 0 || compile_c || compile_wasm)) {
        throw std::runtime_error("--config is only valid for REPL mode");
    }

    if (scripts.empty() && ctx.linker.size() == 0) {
        std::unordered_map<std::string, std::string> repl_cfg;
        if (!config_path.empty()) {
            repl_cfg = klyspec_parse(config_path);
        }
        const std::string prompt = klyspec_get(repl_cfg, "prompt", "qamrpp> ");
        const std::string history_file = klyspec_get(repl_cfg, "history_file", "");
        const std::string initial_commands = klyspec_get(repl_cfg, "initial_commands", "");

        qamrpp::Readline rl;
        rl.set_completer([](const std::string& prefix) -> std::vector<std::string> {
            std::vector<std::string> words;
            const char* builtins[] = {
                "and", "do", "else", "elseif", "end", "exit", "false", "for",
                "function", "help", "if", "in", "local", "nil", "not", "or",
                "quit", "return", "then", "true", "while"
            };
            for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i) {
                words.push_back(builtins[i]);
            }
            std::vector<std::string> out;
            for (size_t i = 0; i < words.size(); ++i) {
                if (words[i].find(prefix) == 0) out.push_back(words[i]);
            }
            return out;
        });
        (void)rl.load_syntax("polyrl.syntax");
        if (!history_file.empty()) rl.load_history(history_file);
        if (!initial_commands.empty()) {
            (void)run_report(ctx, initial_commands, "initial command error");
        }
        while (true) {
            std::string source;
            while (true) {
                const std::string current_prompt = source.empty() ? prompt : std::string("... ");
                const std::string initial = source.empty() ? std::string() : repl_initial_indent(source);
                std::string line;
                const bool got_line = initial.empty()
                    ? rl.read_line(current_prompt, line)
                    : rl.read_line(current_prompt, line, initial);
                if (!got_line) {
                    if (!history_file.empty()) rl.save_history(history_file);
                    if (source.empty()) return 0;
                    break;
                }
                const std::string command = trim_copy(line);
                if (source.empty() && (command == "exit" || command == "quit")) {
                    if (!history_file.empty()) rl.save_history(history_file);
                    return 0;
                }
                if (source.empty() && command.empty()) break;
                if (!source.empty()) source.push_back('\n');
                source += line;
                if (!repl_needs_more_input(source)) break;
            }
            if (trim_copy(source).empty()) continue;
            rl.add_history(source);
            (void)run_report(ctx, source, "error");
        }
        if (!history_file.empty()) rl.save_history(history_file);
        return 0;
    }

    for (size_t i = 0; i < scripts.size(); ++i) {
        if (!ctx.linker.add_file(scripts[i])) throw std::runtime_error("cannot open script: " + scripts[i]);
    }
    std::string linked_source;
    for (size_t i = 0; i < qbf_sources.size(); ++i) {
        linked_source += qbf_sources[i];
        if (!qbf_sources[i].empty() && qbf_sources[i].back() != '\n') linked_source.push_back('\n');
    }
    for (size_t i = 0; i < compile_units.size(); ++i) {
        std::ifstream in(compile_units[i].c_str(), std::ios::in | std::ios::binary);
        if (!in.good()) throw std::runtime_error("cannot open script: " + compile_units[i]);
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        linked_source += src;
        if (!src.empty() && src.back() != '\n') linked_source.push_back('\n');
    }

    if (compile_c || compile_wasm) {
        std::vector<qamrpp::ValuePtr> args;
        args.push_back(std::make_shared<qamrpp::Value>(linked_source));
        qamrpp::ValuePtr compiled = compile_c
            ? call_native(ctx, "compile_to_c", args)
            : call_native(ctx, "compile_to_wasm", args);
        const std::string out_text = compiled ? compiled->to_string() : std::string();
        if (compile_out.empty()) {
            std::cout << out_text << "\n";
        } else {
            std::ofstream out(compile_out.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
            if (!out.good()) throw std::runtime_error("cannot write compile output: " + compile_out);
            out << out_text;
        }
        return 0;
    }

    try {
        (void)ctx.linker.link(ctx);
    } catch (const std::exception& e) {
        std::cerr << "qamrpp-cli: " << e.what() << "\n";
        return 1;
    }

    if (serialize) {
        std::vector<qamrpp::ValuePtr> noargs;
        qamrpp::ValuePtr v = call_native(ctx, "get_serialized_program", std::move(noargs));
        std::cout << v->to_string() << "\n";
    }
    return 0;
    } catch (const std::exception& e) {
        std::cerr << "qamrpp-cli: " << e.what() << "\n";
        return 1;
    }
}
