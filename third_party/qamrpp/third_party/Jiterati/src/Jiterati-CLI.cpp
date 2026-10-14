#include "Jiterati.hpp"
#include "BE/Metadata.hpp"
#include "IR/IR.hpp"
#include "IR/ICompiler.hpp"
#include "zstd/zstd.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char archive_magic[] = "JPKG1\n";

struct CliError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Options {
    std::vector<std::string> args;
};

struct Manifest {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> plugins;
    std::vector<std::string> passes;
    std::vector<std::string> backends;
    std::vector<std::string> files;
};

bool safe_relative_path(std::string const& path);

std::string read_file(fs::path const& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw CliError("unable to read file: " + path.string());
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

fs::path temporary_path(std::string const& stem) {
    static unsigned counter = 0;
    return fs::temp_directory_path() / (stem + "-" + std::to_string(++counter) + ".tmp");
}

void write_file(fs::path const& path, std::string const& text) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) throw CliError("unable to write file: " + path.string());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

bool has_arg(Options const& options, std::string const& name) {
    return std::find(options.args.begin(), options.args.end(), name) != options.args.end();
}

std::optional<std::string> value_after(Options const& options, std::string const& name) {
    for (std::size_t i = 0; i + 1 < options.args.size(); ++i) {
        if (options.args[i] == name) return options.args[i + 1];
    }
    return std::nullopt;
}

std::vector<std::string> positional_args(Options const& options) {
    std::set<std::string> takes_value = {
        "--out", "--target", "--emit", "--pack", "--unpack", "--validate", "--view-manifest",
        "--list-plugins", "--list-passes", "--list-backends", "--install", "--remove", "--upgrade", "--verify",
        "--set", "--get"
    };
    std::vector<std::string> result;
    for (std::size_t i = 0; i < options.args.size(); ++i) {
        auto const& arg = options.args[i];
        if (takes_value.count(arg) != 0) {
            ++i;
            continue;
        }
        if (!arg.empty() && arg.front() == '-') continue;
        result.push_back(arg);
    }
    return result;
}

void print_global_help(std::ostream& out) {
    out << "jiterati-cli 0.1.0\n\n"
        << "Usage: jiterati-cli <command> [options]\n\n"
        << "Commands:\n"
        << "  compile      Validate IR and emit assembly, textual IR, object placeholder, or executable placeholder\n"
        << "  parse        Parse textual IR and report a normalized summary\n"
        << "  validate     Validate textual IR\n"
        << "  format       Format textual IR\n"
        << "  package      Pack, unpack, validate, inspect, install, remove, upgrade, and repair packages\n"
        << "  plugin       List plugin metadata known to this build\n"
        << "  backend      List backend targets known to this build\n"
        << "  pass         List pass metadata known to this build\n"
        << "  config       Read and write simple user configuration\n"
        << "  cache        Inspect or clear the Jiterati cache directory\n"
        << "  doctor       Check local tool and home-directory configuration\n"
        << "  version      Print version information\n"
        << "  help         Print this help or command-specific help\n";
}

void print_command_help(std::string const& command, std::ostream& out) {
    if (command == "compile") {
        out << "Usage: jiterati-cli compile [--target amd64|aarch64|rv64|wasm] [--emit asm|ir|obj|exe] [--out PATH] FILE\n";
    } else if (command == "parse") {
        out << "Usage: jiterati-cli parse FILE\n";
    } else if (command == "validate") {
        out << "Usage: jiterati-cli validate FILE\n";
    } else if (command == "format") {
        out << "Usage: jiterati-cli format [--out PATH] FILE\n";
    } else if (command == "package") {
        out << "Usage: jiterati-cli package [action]\n\n"
            << "Actions:\n"
            << "  --pack package.sh --out Foo.jpkg\n"
            << "  --unpack Foo.jpkg [--out DIR]\n"
            << "  --validate Foo.jpkg\n"
            << "  --view-manifest Foo.jpkg\n"
            << "  --list-plugins Foo.jpkg\n"
            << "  --list-passes Foo.jpkg\n"
            << "  --list-backends Foo.jpkg\n"
            << "  --install Foo.jpkg\n"
            << "  --remove NAME\n"
            << "  --upgrade Foo.jpkg\n"
            << "  --list-installed\n"
            << "  --verify NAME\n"
            << "  --repair\n";
    } else if (command == "plugin") {
        out << "Usage: jiterati-cli plugin --list\n";
    } else if (command == "backend") {
        out << "Usage: jiterati-cli backend --list\n";
    } else if (command == "pass") {
        out << "Usage: jiterati-cli pass --list\n";
    } else if (command == "config") {
        out << "Usage: jiterati-cli config [--home] [--get KEY] [--set KEY=VALUE]\n";
    } else if (command == "cache") {
        out << "Usage: jiterati-cli cache [--path] [--clear]\n";
    } else if (command == "doctor") {
        out << "Usage: jiterati-cli doctor\n";
    } else if (command == "version") {
        out << "Usage: jiterati-cli version\n";
    } else {
        print_global_help(out);
    }
}

fs::path jiterati_home() {
    if (char const* env = std::getenv("JITERATI_HOME")) return fs::path(env);
    if (char const* home = std::getenv("HOME")) return fs::path(home) / ".local" / "jiterati";
    return fs::current_path() / ".jiterati";
}

std::string json_string_value(std::string const& json, std::string const& key) {
    std::string needle = '"' + key + '"';
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    auto end = pos + 1;
    while (true) {
        end = json.find('"', end);
        if (end == std::string::npos) return {};
        if (json[end - 1] != '\\') break;
        ++end;
    }
    return json.substr(pos + 1, end - pos - 1);
}

std::vector<std::string> json_array_values(std::string const& json, std::string const& key) {
    std::vector<std::string> values;
    std::string needle = '"' + key + '"';
    auto pos = json.find(needle);
    if (pos == std::string::npos) return values;
    pos = json.find('[', pos + needle.size());
    auto end = json.find(']', pos == std::string::npos ? 0 : pos);
    if (pos == std::string::npos || end == std::string::npos) return values;
    std::string_view body(json.data() + pos + 1, end - pos - 1);
    std::size_t cursor = 0;
    while (cursor < body.size()) {
        auto quote = body.find('"', cursor);
        if (quote == std::string_view::npos) break;
        auto close = body.find('"', quote + 1);
        if (close == std::string_view::npos) break;
        values.emplace_back(body.substr(quote + 1, close - quote - 1));
        cursor = close + 1;
    }
    return values;
}

Manifest parse_manifest(std::string const& text) {
    Manifest manifest;
    manifest.name = json_string_value(text, "name");
    manifest.version = json_string_value(text, "version");
    manifest.description = json_string_value(text, "description");
    manifest.plugins = json_array_values(text, "plugins");
    manifest.passes = json_array_values(text, "passes");
    manifest.backends = json_array_values(text, "backends");
    manifest.files = json_array_values(text, "files");
    if (manifest.name.empty()) throw CliError("manifest is missing required string field: name");
    if (manifest.version.empty()) throw CliError("manifest is missing required string field: version");
    return manifest;
}

std::set<std::string> package_payload_paths(std::map<std::string, std::string> const& entries) {
    std::set<std::string> paths;
    for (auto const& [path, content] : entries) {
        (void)content;
        if (path != "META-INF/manifest.json") paths.insert(path);
    }
    return paths;
}

void validate_manifest_files(Manifest const& manifest, std::map<std::string, std::string> const& entries) {
    std::set<std::string> declared(manifest.files.begin(), manifest.files.end());
    auto payload = package_payload_paths(entries);
    for (auto const& path : declared) {
        if (!safe_relative_path(path)) throw CliError("manifest contains unsafe file path: " + path);
        if (entries.find(path) == entries.end()) throw CliError("manifest lists missing file: " + path);
    }
    for (auto const& path : payload) {
        if (declared.find(path) == declared.end()) throw CliError("manifest omits package file: " + path);
    }
}

std::string read_manifest_from_directory(fs::path const& root) {
    auto path = root / "META-INF" / "manifest.json";
    if (!fs::exists(path)) throw CliError("package root is missing META-INF/manifest.json: " + root.string());
    return read_file(path);
}

std::map<std::string, std::string> read_raw_archive(fs::path const& raw_archive) {
    std::ifstream input(raw_archive, std::ios::binary);
    if (!input) throw CliError("unable to read package archive: " + raw_archive.string());
    std::string magic;
    magic.resize(sizeof(archive_magic) - 1);
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (magic != archive_magic) throw CliError("invalid package magic: " + raw_archive.string());
    std::map<std::string, std::string> entries;
    while (input) {
        std::string line;
        if (!std::getline(input, line)) break;
        if (line.empty()) continue;
        auto sep = line.find(' ');
        if (sep == std::string::npos) throw CliError("malformed package entry header");
        std::string path = line.substr(0, sep);
        std::size_t size = static_cast<std::size_t>(std::stoull(line.substr(sep + 1)));
        std::string content(size, '\0');
        input.read(content.data(), static_cast<std::streamsize>(size));
        if (static_cast<std::size_t>(input.gcount()) != size) throw CliError("truncated package entry: " + path);
        char newline = '\0';
        input.get(newline);
        if (newline != '\n') throw CliError("malformed package entry terminator: " + path);
        entries[path] = std::move(content);
    }
    if (entries.find("META-INF/manifest.json") == entries.end()) {
        throw CliError("package is missing META-INF/manifest.json");
    }
    return entries;
}

std::map<std::string, std::string> read_archive(fs::path const& package) {
    std::string data = read_file(package);
    if (data.rfind(archive_magic, 0) == 0) return read_raw_archive(package);
    fs::path raw = temporary_path("jiterati-jpkg-read");
    jiterati::third_party::zstd::decompress(package, raw);
    auto entries = read_raw_archive(raw);
    fs::remove(raw);
    return entries;
}

void write_archive(fs::path const& package, fs::path const& root) {
    auto manifest_text = read_manifest_from_directory(root);
    auto manifest = parse_manifest(manifest_text);
    std::vector<fs::path> files;
    for (auto const& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() == "package.sh") continue;
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    std::map<std::string, std::string> pending_entries;
    for (auto const& file : files) {
        auto rel = fs::relative(file, root).generic_string();
        pending_entries[rel] = read_file(file);
    }
    validate_manifest_files(manifest, pending_entries);
    if (package.has_parent_path()) fs::create_directories(package.parent_path());
    fs::path raw = temporary_path("jiterati-jpkg-write");
    std::ofstream output(raw, std::ios::binary);
    if (!output) throw CliError("unable to write temporary package: " + raw.string());
    output << archive_magic;
    for (auto const& [rel, data] : pending_entries) {
        output << rel << ' ' << data.size() << '\n';
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
        output << '\n';
    }
    output.close();
    jiterati::third_party::zstd::compress(raw, package);
    fs::remove(raw);
    (void)manifest;
}

bool safe_relative_path(std::string const& path) {
    if (path.empty() || path.front() == '/') return false;
    fs::path fs_path(path);
    for (auto const& part : fs_path) {
        if (part == "..") return false;
    }
    return true;
}

void extract_archive(fs::path const& package, fs::path const& output_dir) {
    auto entries = read_archive(package);
    fs::create_directories(output_dir);
    for (auto const& [path, content] : entries) {
        if (!safe_relative_path(path)) throw CliError("unsafe package path: " + path);
        write_file(output_dir / fs::path(path), content);
    }
}

Manifest manifest_from_archive(fs::path const& package) {
    auto entries = read_archive(package);
    auto manifest = parse_manifest(entries.at("META-INF/manifest.json"));
    validate_manifest_files(manifest, entries);
    return manifest;
}

std::string manifest_text_from_archive(fs::path const& package) {
    auto entries = read_archive(package);
    parse_manifest(entries.at("META-INF/manifest.json"));
    return entries.at("META-INF/manifest.json");
}

void print_list(std::vector<std::string> const& values) {
    for (auto const& value : values) std::cout << value << '\n';
}

void ensure_home_layout(fs::path const& home) {
    for (auto const& dir : {"bin", "plugins", "passes", "backends", "packages", "cache", "logs", "docs", "examples"}) {
        fs::create_directories(home / dir);
    }
}

int parse_ir_file(fs::path const& file, bool verbose) {
    auto text = read_file(file);
    std::string error;
    auto module = jiterati::parse_jbl(text, &error);
    if (module) {
        if (verbose) std::cout << "parsed JBL module with " << module->functions().size() << " function(s)\n";
        return 0;
    }
    std::vector<jiterati::ir::Diagnostic> diagnostics;
    auto terse = jiterati::ir::parse_terse_module(text, &diagnostics);
    if (terse) {
        if (verbose) std::cout << "parsed terse module " << terse->name << " with " << terse->functions.size() << " function(s)\n";
        return 0;
    }
    std::cerr << file << ": error: failed to parse textual IR\n";
    if (!error.empty()) std::cerr << file << ": note: JBL parser: " << error << '\n';
    for (auto const& diagnostic : diagnostics) {
        std::cerr << file << ": " << jiterati::ir::diagnostic_str(diagnostic) << '\n';
    }
    return 1;
}

std::string format_ir_file(fs::path const& file);

std::unique_ptr<jiterati::ir::ICompiler> create_ir_compiler(std::string const& target) {
    if (target == "amd64") return jiterati::ir::create_amd64_ir_compiler();
    if (target == "aarch64") return jiterati::ir::create_aarch64_ir_compiler();
    if (target == "rv64") return jiterati::ir::create_rv64_ir_compiler();
    if (target == "wasm") return jiterati::ir::create_wasm_ir_compiler();
    throw CliError("unknown target: " + target);
}

void print_ir_diagnostics(fs::path const& file, std::vector<jiterati::ir::Diagnostic> const& diagnostics) {
    for (auto const& diagnostic : diagnostics) {
        std::cerr << file << ": " << jiterati::ir::diagnostic_str(diagnostic) << '\n';
    }
}

std::string compile_ir_text(fs::path const& file, std::string const& target, std::string const& emit) {
    auto text = read_file(file);
    if (emit == "ir") return format_ir_file(file);

    std::vector<jiterati::ir::Diagnostic> diagnostics;
    auto terse = jiterati::ir::parse_terse_module(text, &diagnostics);
    if (!terse) {
        if (!diagnostics.empty()) {
            std::cerr << file << ": error: failed to parse terse IR\n";
            print_ir_diagnostics(file, diagnostics);
            std::string error;
            if (jiterati::parse_jbl(text, &error)) {
                throw CliError("compile accepts terse IR only; JBL input is parse/format only");
            }
            throw CliError("failed to parse input");
        }
        throw CliError("failed to parse input");
    }

    auto compiler = create_ir_compiler(target);
    auto artifact = compiler->compile(*terse);
    if (!artifact.ok()) {
        std::cerr << file << ": error: failed to compile textual IR\n";
        print_ir_diagnostics(file, artifact.diagnostics);
        throw CliError("compilation failed");
    }
    return artifact.text;
}

std::string format_ir_file(fs::path const& file) {
    auto text = read_file(file);
    std::string error;
    auto module = jiterati::parse_jbl(text, &error);
    if (module) return jiterati::print_jbl(*module);
    std::vector<jiterati::ir::Diagnostic> diagnostics;
    auto terse = jiterati::ir::parse_terse_module(text, &diagnostics);
    if (terse) return terse->str();
    throw CliError("failed to parse input for formatting: " + file.string());
}

int command_parse(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("parse", std::cout); return 0; }
    auto pos = positional_args(options);
    if (pos.size() != 1) throw CliError("parse expects exactly one input file");
    return parse_ir_file(pos[0], true);
}

int command_validate(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("validate", std::cout); return 0; }
    auto pos = positional_args(options);
    if (pos.size() != 1) throw CliError("validate expects exactly one input file");
    int result = parse_ir_file(pos[0], false);
    if (result == 0) std::cout << pos[0] << ": ok\n";
    return result;
}

int command_format(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("format", std::cout); return 0; }
    auto pos = positional_args(options);
    if (pos.size() != 1) throw CliError("format expects exactly one input file");
    auto formatted = format_ir_file(pos[0]);
    if (auto out = value_after(options, "--out")) write_file(*out, formatted);
    else std::cout << formatted;
    return 0;
}

int command_compile(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("compile", std::cout); return 0; }
    auto pos = positional_args(options);
    if (pos.size() != 1) throw CliError("compile expects exactly one input file");
    std::string target = value_after(options, "--target").value_or("amd64");
    std::string emit = value_after(options, "--emit").value_or("asm");
    std::set<std::string> targets = {"amd64", "aarch64", "rv64", "wasm"};
    std::set<std::string> emits = {"asm", "ir", "obj", "exe"};
    if (targets.count(target) == 0) throw CliError("unknown target: " + target);
    if (emits.count(emit) == 0) throw CliError("unknown emit kind: " + emit);
    std::ostringstream output;
    if (emit == "asm" || emit == "ir") {
        output << compile_ir_text(pos[0], target, emit);
    } else {
        output << "; jiterati " << emit << " placeholder for target " << target << "\n; source: " << pos[0] << "\n";
    }
    if (auto out = value_after(options, "--out")) write_file(*out, output.str());
    else std::cout << output.str();
    return 0;
}

int command_package(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("package", std::cout); return 0; }
    if (auto pack = value_after(options, "--pack")) {
        auto out = value_after(options, "--out").value_or(fs::path(*pack).stem().string() + ".jpkg");
        auto root = fs::absolute(fs::path(*pack)).parent_path();
        write_archive(out, root);
        std::cout << "packed " << out << '\n';
        return 0;
    }
    if (auto unpack = value_after(options, "--unpack")) {
        auto out = value_after(options, "--out").value_or(fs::path(*unpack).stem().string());
        extract_archive(*unpack, out);
        std::cout << "unpacked " << *unpack << " to " << out << '\n';
        return 0;
    }
    if (auto validate = value_after(options, "--validate")) {
        auto manifest = manifest_from_archive(*validate);
        std::cout << *validate << ": ok (" << manifest.name << ' ' << manifest.version << ")\n";
        return 0;
    }
    if (auto view = value_after(options, "--view-manifest")) {
        std::cout << manifest_text_from_archive(*view);
        return 0;
    }
    if (auto list = value_after(options, "--list-plugins")) { print_list(manifest_from_archive(*list).plugins); return 0; }
    if (auto list = value_after(options, "--list-passes")) { print_list(manifest_from_archive(*list).passes); return 0; }
    if (auto list = value_after(options, "--list-backends")) { print_list(manifest_from_archive(*list).backends); return 0; }
    if (auto install = value_after(options, "--install")) {
        auto manifest = manifest_from_archive(*install);
        auto home = jiterati_home();
        ensure_home_layout(home);
        auto package_dir = home / "packages" / manifest.name;
        if (fs::exists(package_dir)) fs::remove_all(package_dir);
        extract_archive(*install, package_dir);
        std::cout << "installed " << manifest.name << " to " << package_dir << '\n';
        return 0;
    }
    if (auto upgrade = value_after(options, "--upgrade")) {
        Options nested{{"--install", *upgrade}};
        return command_package(nested);
    }
    if (auto remove = value_after(options, "--remove")) {
        auto package_dir = jiterati_home() / "packages" / *remove;
        if (!fs::exists(package_dir)) throw CliError("package is not installed: " + *remove);
        fs::remove_all(package_dir);
        std::cout << "removed " << *remove << '\n';
        return 0;
    }
    if (has_arg(options, "--list-installed")) {
        auto packages = jiterati_home() / "packages";
        if (!fs::exists(packages)) return 0;
        for (auto const& entry : fs::directory_iterator(packages)) {
            if (entry.is_directory()) std::cout << entry.path().filename().string() << '\n';
        }
        return 0;
    }
    if (auto verify = value_after(options, "--verify")) {
        auto manifest = read_manifest_from_directory(jiterati_home() / "packages" / *verify);
        auto parsed = parse_manifest(manifest);
        std::cout << parsed.name << ": ok\n";
        return 0;
    }
    if (has_arg(options, "--repair")) {
        ensure_home_layout(jiterati_home());
        std::cout << "repaired " << jiterati_home() << '\n';
        return 0;
    }
    throw CliError("package expects an action; try --help");
}

int command_plugin(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("plugin", std::cout); return 0; }
    if (!has_arg(options, "--list")) throw CliError("plugin expects --list");
    std::cout << "BURSISel\nMaxMunchISel\nListISched\n";
    return 0;
}

int command_backend(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("backend", std::cout); return 0; }
    if (!has_arg(options, "--list")) throw CliError("backend expects --list");
    bool verbose = has_arg(options, "--verbose");
    for (auto it = jiterati::be::backend_dataset_metadata_begin(); it != jiterati::be::backend_dataset_metadata_end(); ++it) {
        if (verbose) {
            std::cout << it->target << " arch=" << it->dataset_arch << " source=" << it->dataset_source
                      << " instructions=" << it->instruction_count << " mnemonics=" << it->mnemonic_count << '\n';
        } else {
            std::cout << it->target << '\n';
        }
    }
    return 0;
}

int command_pass(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("pass", std::cout); return 0; }
    if (!has_arg(options, "--list")) throw CliError("pass expects --list");
    std::cout << "CFGAnalysis\nDominatorAnalysis\nLivenessAnalysis\nConstantFolding\nConstantPropagation\nDeadCodeRemoval\nStrengthReduction\nPeepholeOptimization\n";
    return 0;
}

int command_config(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("config", std::cout); return 0; }
    if (has_arg(options, "--home")) { std::cout << jiterati_home() << '\n'; return 0; }
    auto config_path = jiterati_home() / "config.txt";
    if (auto set = value_after(options, "--set")) {
        ensure_home_layout(jiterati_home());
        std::ofstream output(config_path, std::ios::app);
        output << *set << '\n';
        return 0;
    }
    if (auto get = value_after(options, "--get")) {
        std::ifstream input(config_path);
        std::string line;
        std::string prefix = *get + "=";
        while (std::getline(input, line)) {
            if (line.rfind(prefix, 0) == 0) std::cout << line.substr(prefix.size()) << '\n';
        }
        return 0;
    }
    throw CliError("config expects --home, --get, or --set");
}

int command_cache(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("cache", std::cout); return 0; }
    auto cache = jiterati_home() / "cache";
    if (has_arg(options, "--path")) { std::cout << cache << '\n'; return 0; }
    if (has_arg(options, "--clear")) {
        if (fs::exists(cache)) fs::remove_all(cache);
        fs::create_directories(cache);
        std::cout << "cleared " << cache << '\n';
        return 0;
    }
    throw CliError("cache expects --path or --clear");
}

int command_doctor(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("doctor", std::cout); return 0; }
    auto home = jiterati_home();
    ensure_home_layout(home);
    std::cout << "jiterati home: " << home << '\n';
    std::cout << "docs target: configured by CMake when Doxygen is available\n";
    std::cout << "package support: deterministic .jpkg archive over zstd: " << (jiterati::third_party::zstd::available() ? "ok" : "missing") << '\n';
    return 0;
}

int command_version(Options const& options) {
    if (has_arg(options, "--help")) { print_command_help("version", std::cout); return 0; }
    std::cout << "jiterati-cli 0.1.0\n";
    return 0;
}

Options collect_options(int argc, char** argv, int begin) {
    Options options;
    for (int i = begin; i < argc; ++i) options.args.emplace_back(argv[i]);
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_global_help(std::cout);
            return 0;
        }
        std::string command = argv[1];
        if (command == "--help" || command == "-h") {
            print_global_help(std::cout);
            return 0;
        }
        if (command == "help") {
            if (argc >= 3) print_command_help(argv[2], std::cout);
            else print_global_help(std::cout);
            return 0;
        }
        auto options = collect_options(argc, argv, 2);
        if (command == "parse") return command_parse(options);
        if (command == "validate") return command_validate(options);
        if (command == "format") return command_format(options);
        if (command == "compile") return command_compile(options);
        if (command == "package") return command_package(options);
        if (command == "plugin") return command_plugin(options);
        if (command == "backend") return command_backend(options);
        if (command == "pass") return command_pass(options);
        if (command == "config") return command_config(options);
        if (command == "cache") return command_cache(options);
        if (command == "doctor") return command_doctor(options);
        if (command == "version") return command_version(options);
        throw CliError("unknown command: " + command);
    } catch (CliError const& error) {
        std::cerr << "jiterati-cli: error: " << error.what() << '\n';
        return 2;
    } catch (std::exception const& error) {
        std::cerr << "jiterati-cli: internal error: " << error.what() << '\n';
        return 3;
    }
}
