#pragma once

#include "Klyspec.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>
#include <vector>

namespace klyspec {



// ---------------------------------------------------------------------------
// CLI manifest generation
//
// A CLI manifest is a YAML document (parsed via yaml-cpp) describing a
// command-line tool: its name, version, summary, and a list of
// options/flags/positionals. From that manifest we can generate standalone
// CLI source files in C and C++, plus manpages and help text.
// ---------------------------------------------------------------------------

struct CLIArgumentSpec {
  std::string id{};
  std::string kind{"option"};            // flag | option | positional | variadic
  std::vector<std::string> names{};
  std::string help{};
  std::optional<std::string> default_value{};
  bool required{false};
};

struct CLICommandSpec {
  std::string name{};
  std::string help{};
  std::vector<CLIArgumentSpec> arguments{};
};

struct CLIManifestSpec {
  std::string program{};
  std::string version{"0.1.0"};
  std::string about{};
  std::vector<CLICommandSpec> commands{};
  std::vector<CLIArgumentSpec> arguments{};
};

struct CLIManifestResult {
  std::optional<CLIManifestSpec> manifest{};
  std::vector<std::string> diagnostics{};
  bool ok{false};
};

namespace detail {

inline auto yaml_expect_scalar(const YAML::Node &node, const char *path, std::vector<std::string> &diag) -> bool {
  if (node && !node.IsScalar()) {
    diag.push_back(std::string("CLI manifest field '") + path + "' must be a scalar");
    return false;
  }
  return true;
}

inline auto is_valid_kind(std::string_view kind) -> bool {
  return kind == "flag" || kind == "option" || kind == "positional" || kind == "variadic";
}

inline auto yaml_as_string(const YAML::Node &node) -> std::string {
  if (!node || !node.IsScalar()) return {};
  try {
    return node.as<std::string>();
  } catch (const YAML::Exception &) {
    return node.Scalar();
  }
}

inline auto yaml_as_bool(const YAML::Node &node, bool fallback) -> bool {
  if (!node) return fallback;
  try {
    return node.as<bool>();
  } catch (const YAML::Exception &) {
    return fallback;
  }
}

inline auto parse_yaml_argument(const YAML::Node &value) -> CLIArgumentSpec {
  CLIArgumentSpec spec{};
  if (!value || !value.IsMap()) return spec;
  if (value["id"]) spec.id = yaml_as_string(value["id"]);
  if (value["kind"]) spec.kind = yaml_as_string(value["kind"]);
  if (value["names"] && value["names"].IsSequence()) {
    for (const auto &name : value["names"]) {
      if (name && name.IsScalar()) spec.names.push_back(yaml_as_string(name));
    }
  }
  if (value["help"]) spec.help = yaml_as_string(value["help"]);
  if (value["default"]) spec.default_value = yaml_as_string(value["default"]);
  if (value["required"]) spec.required = yaml_as_bool(value["required"], false);
  return spec;
}

inline auto parse_yaml_arguments(const YAML::Node &value) -> std::vector<CLIArgumentSpec> {
  std::vector<CLIArgumentSpec> out{};
  if (!value || !value.IsSequence()) return out;
  for (const auto &item : value) {
    out.push_back(parse_yaml_argument(item));
  }
  return out;
}

inline auto parse_yaml_command(const YAML::Node &value) -> CLICommandSpec {
  CLICommandSpec spec{};
  if (!value || !value.IsMap()) return spec;
  if (value["name"]) spec.name = yaml_as_string(value["name"]);
  if (value["help"]) spec.help = yaml_as_string(value["help"]);
  if (value["arguments"]) spec.arguments = parse_yaml_arguments(value["arguments"]);
  return spec;
}

inline auto parse_yaml_commands(const YAML::Node &value) -> std::vector<CLICommandSpec> {
  std::vector<CLICommandSpec> out{};
  if (!value || !value.IsSequence()) return out;
  for (const auto &item : value) {
    out.push_back(parse_yaml_command(item));
  }
  return out;
}

inline auto validate_manifest_arguments(const std::vector<CLIArgumentSpec> &arguments,
                                      std::vector<std::string> &diagnostics,
                                      std::string_view command) -> bool {
  bool ok = true;
  std::unordered_set<std::string> ids{};
  std::unordered_set<std::string> names{};
  for (const auto &arg : arguments) {
    const auto prefix = command.empty() ? "argument" : std::string("command '") + std::string(command) + "' argument";
    if (arg.id.empty()) {
      diagnostics.push_back("manifest " + prefix + " is missing required field: id");
      ok = false;
      continue;
    }
    if (!ids.insert(arg.id).second) {
      diagnostics.push_back("manifest " + prefix + " id '" + arg.id + "' is duplicated");
      ok = false;
    }
    if (!is_valid_kind(arg.kind)) {
      diagnostics.push_back("manifest " + prefix + " '" + arg.id + "' has invalid kind '" + arg.kind + "'");
      ok = false;
    }
    if ((arg.kind == "flag" || arg.kind == "option") && arg.names.empty()) {
      diagnostics.push_back("manifest " + prefix + " '" + arg.id + "' must provide at least one name");
      ok = false;
    }
    if (arg.kind == "flag" && arg.required) {
      diagnostics.push_back("manifest " + prefix + " '" + arg.id + "' cannot be required when kind is flag");
      ok = false;
    }
    for (const auto &name : arg.names) {
      if (name.empty()) {
        diagnostics.push_back("manifest " + prefix + " '" + arg.id + "' has empty name");
        ok = false;
        continue;
      }
      if (!names.insert(name).second) {
        diagnostics.push_back("manifest " + prefix + " name '" + name + "' is duplicated");
        ok = false;
      }
    }
  }
  return ok;
}

inline auto validate_manifest_spec(const CLIManifestSpec &manifest, std::vector<std::string> &diagnostics) -> bool {
  bool ok = true;
  if (!validate_manifest_arguments(manifest.arguments, diagnostics, "")) {
    ok = false;
  }
  std::unordered_set<std::string> command_names{};
  for (const auto &command : manifest.commands) {
    if (command.name.empty()) {
      diagnostics.push_back("manifest command is missing required field: name");
      ok = false;
      continue;
    }
    if (!command_names.insert(command.name).second) {
      diagnostics.push_back("manifest command name '" + command.name + "' is duplicated");
      ok = false;
    }
    if (!validate_manifest_arguments(command.arguments, diagnostics, command.name)) {
      ok = false;
    }
  }
  return ok;
}

} // namespace detail

// Load a CLI manifest from an already-parsed yaml-cpp node. The root must be a
// mapping (e.g. `program: ...`, `arguments: [...]`).
inline auto manifest_from_node(const YAML::Node &root) -> CLIManifestResult {
  CLIManifestResult result{};
  if (!root || !root.IsMap()) {
    result.diagnostics.push_back("CLI manifest root must be a YAML mapping");
    return result;
  }
  CLIManifestSpec manifest{};
  if (root["program"] && !detail::yaml_expect_scalar(root["program"], "program", result.diagnostics)) {
    return result;
  }
  if (root["version"] && !detail::yaml_expect_scalar(root["version"], "version", result.diagnostics)) {
    return result;
  }
  if (root["about"] && !detail::yaml_expect_scalar(root["about"], "about", result.diagnostics)) {
    return result;
  }
  if (root["arguments"] && !root["arguments"].IsSequence()) {
    result.diagnostics.push_back("CLI manifest field 'arguments' must be an array");
    return result;
  }
  if (root["commands"] && !root["commands"].IsSequence()) {
    result.diagnostics.push_back("CLI manifest field 'commands' must be an array");
    return result;
  }

  if (root["program"]) manifest.program = detail::yaml_as_string(root["program"]);
  if (root["version"]) manifest.version = detail::yaml_as_string(root["version"]);
  if (root["about"]) manifest.about = detail::yaml_as_string(root["about"]);
  if (root["arguments"]) manifest.arguments = detail::parse_yaml_arguments(root["arguments"]);
  if (root["commands"]) manifest.commands = detail::parse_yaml_commands(root["commands"]);

  if (manifest.program.empty()) {
    result.diagnostics.push_back("CLI manifest is missing required field: program");
    return result;
  }
  if (!detail::validate_manifest_spec(manifest, result.diagnostics)) {
    return result;
  }
  result.ok = true;
  result.manifest = std::move(manifest);
  return result;
}

// Load a CLI manifest from YAML text. The manifest format is YAML; the `path`
// argument is retained only for diagnostics/context.
inline auto load_cli_manifest(std::string_view source, std::string_view path) -> CLIManifestResult {
  try {
    const auto root = YAML::Load(std::string(source));
    return manifest_from_node(root);
  } catch (const YAML::Exception &error) {
    CLIManifestResult result{};
    result.diagnostics.push_back(std::string("failed to parse CLI manifest") +
                                 (path.empty() ? "" : std::string(" (") + std::string(path) + ")") +
                                 ": " + error.what());
    return result;
  } catch (const std::exception &error) {
    CLIManifestResult result{};
    result.diagnostics.push_back(std::string("failed to parse CLI manifest") +
                                 (path.empty() ? "" : std::string(" (") + std::string(path) + ")") +
                                 ": " + error.what());
    return result;
  }
}

inline auto load_cli_manifest_file(const std::string &path) -> CLIManifestResult {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    CLIManifestResult result{};
    result.diagnostics.push_back("could not open manifest file: " + path);
    return result;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return load_cli_manifest(buffer.str(), path);
}

// ---------------------------------------------------------------------------
// Code/text generation
// ---------------------------------------------------------------------------

namespace detail {

inline auto c_escape(std::string_view text) -> std::string {
  std::string out{};
  out.reserve(text.size() + 2);
  for (const auto c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      case '\r': out += "\\r"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

inline auto argument_spec_lines(const CLIArgumentSpec &spec, std::size_t index) -> std::vector<std::string> {
  std::vector<std::string> lines{};
  std::string names;
  for (std::size_t i = 0; i < spec.names.size(); ++i) {
    if (i != 0) names += " ";
    names += spec.names[i];
  }
  lines.push_back("    {");
  lines.push_back("        /* index  */ " + std::to_string(index) + ",");
  lines.push_back("        /* id     */ \"" + c_escape(spec.id) + "\",");
  lines.push_back("        /* kind   */ \"" + c_escape(spec.kind) + "\",");
  lines.push_back("        /* names  */ \"" + c_escape(names) + "\",");
  lines.push_back("        /* help   */ \"" + c_escape(spec.help) + "\",");
  lines.push_back("        /* def    */ \"" + c_escape(spec.default_value.value_or("")) + "\",");
  lines.push_back("        /* has_def*/ " + std::string(spec.default_value.has_value() ? "1" : "0") + ",");
  lines.push_back("        /* req    */ " + std::string(spec.required ? "1" : "0"));
  lines.push_back("    }");
  return lines;
}

inline auto emit_arguments_table(std::ostringstream &out, const std::vector<CLIArgumentSpec> &args,
                                 const std::string &type, const std::string &name) -> void {
  out << "static const " << type << " " << name << "[" << (args.empty() ? 1 : args.size()) << "] = {\n";
  if (args.empty()) {
    out << "    { 0, \"\", \"\", \"\", \"\", \"\", 0, 0 } /* placeholder */\n";
  } else {
    for (std::size_t i = 0; i < args.size(); ++i) {
      for (const auto &line : argument_spec_lines(args[i], i)) out << line << "\n";
      if (i + 1 != args.size()) out << ",\n";
    }
    out << "\n";
  }
  out << "};\n";
}

} // namespace detail

// Generate a standalone C source file implementing the CLI described by
// `manifest`. The output parses argv, applies defaults, and prints help.
inline auto generate_cli_c(const CLIManifestSpec &manifest) -> std::string {
  std::ostringstream out;
  out << "/* Auto-generated by Klyspec. Do not edit by hand. */\n";
  out << "#include <stdio.h>\n#include <string.h>\n\n";
  out << "typedef struct {\n";
  out << "    int index;\n";
  out << "    const char *id;\n";
  out << "    const char *kind;\n";
  out << "    const char *names;\n";
  out << "    const char *help;\n";
  out << "    const char *default_value;\n";
  out << "    int has_default;\n";
  out << "    int required;\n";
  out << "} klyspec_arg;\n\n";
  out << "static const char *klyspec_program = \"" << detail::c_escape(manifest.program) << "\";\n";
  out << "static const char *klyspec_version = \"" << detail::c_escape(manifest.version) << "\";\n";
  out << "static const char *klyspec_about = \"" << detail::c_escape(manifest.about) << "\";\n\n";
  detail::emit_arguments_table(out, manifest.arguments, "klyspec_arg", "klyspec_arguments");
  out << "static const size_t klyspec_argument_count = " << manifest.arguments.size() << ";\n\n";

  out << "static void klyspec_print_help(void) {\n";
  out << "    printf(\"%s %s\\n%s\\n\\nUsage:\\n  %s [options]\\n\\nOptions:\\n\",\n";
  out << "           klyspec_program, klyspec_version, klyspec_about, klyspec_program);\n";
  out << "    for (size_t i = 0; i < klyspec_argument_count; ++i) {\n";
  out << "        const klyspec_arg *a = &klyspec_arguments[i];\n";
  out << "        printf(\"  %-16s %s\\n\", (a->names && a->names[0]) ? a->names : a->id, a->help);\n";
  out << "    }\n";
  out << "}\n\n";

  out << "int main(int argc, char **argv) {\n";
  out << "    for (int i = 1; i < argc; ++i) {\n";
  out << "        if (strcmp(argv[i], \"-h\") == 0 || strcmp(argv[i], \"--help\") == 0) {\n";
  out << "            klyspec_print_help();\n";
  out << "            return 0;\n";
  out << "        }\n";
  out << "        if (strcmp(argv[i], \"--version\") == 0) {\n";
  out << "            printf(\"%s %s\\n\", klyspec_program, klyspec_version);\n";
  out << "            return 0;\n";
  out << "        }\n";
  out << "    }\n";
  out << "    klyspec_print_help();\n";
  out << "    return 0;\n";
  out << "}\n";
  return out.str();
}

// Generate a standalone C++ source file implementing the CLI. It builds a
// klyspec::Registry from the manifest and parses argv through KlyCLIService,
// dogfooding the project's own parser.
inline auto generate_cli_cpp(const CLIManifestSpec &manifest) -> std::string {
  std::ostringstream out;
  out << "// Auto-generated by Klyspec. Do not edit by hand.\n";
  out << "#include \"Klyspec.hpp\"\n\n";
  out << "#include <iostream>\n#include <string>\n#include <vector>\n\n";
  out << "static klyspec::Registry build_registry() {\n";
  out << "    klyspec::Registry registry;\n";
  out << "    klyspec::CommandSpec command;\n";
  out << "    command.name = \"" << detail::c_escape(manifest.program) << "\";\n";
  out << "    command.help = \"" << detail::c_escape(manifest.about) << "\";\n";
  out << "    registry.register_command(command);\n";
  for (const auto &arg : manifest.arguments) {
    out << "    {\n";
    out << "        klyspec::ArgumentSpec spec{};\n";
    out << "        spec.id = \"" << detail::c_escape(arg.id) << "\";\n";
    const std::string resolved_kind =
        arg.kind == "positional" ? "klyspec::ArgumentKind::positional"
        : arg.kind == "variadic" ? "klyspec::ArgumentKind::variadic"
        : arg.kind == "flag" ? "klyspec::ArgumentKind::flag"
        : "klyspec::ArgumentKind::option";
    const bool is_flag = (resolved_kind == "klyspec::ArgumentKind::flag");
    out << "        spec.kind = " << resolved_kind << ";\n";
    out << "        spec.value_policy = " << (is_flag ? "klyspec::ValuePolicy::none" : "klyspec::ValuePolicy::required") << ";\n";
    out << "        spec.names = {";
    for (std::size_t i = 0; i < arg.names.size(); ++i) {
      if (i != 0) out << ", ";
      out << "\"" << detail::c_escape(arg.names[i]) << "\"";
    }
    out << "};\n";
    out << "        spec.help = \"" << detail::c_escape(arg.help) << "\";\n";
    out << "        spec.required = " << (arg.required ? "true" : "false") << ";\n";
    if (arg.default_value.has_value()) {
      out << "        spec.default_value = \"" << detail::c_escape(*arg.default_value) << "\";\n";
    }
    out << "        registry.register_argument(command.name, spec);\n";
    out << "    }\n";
  }
  out << "    return registry;\n";
  out << "}\n\n";
  out << "int main(int argc, char **argv) {\n";
  out << "    auto registry = build_registry();\n";
  out << "    klyspec::KlyCLIService cli(registry);\n";
  out << "    std::vector<std::string> args;\n";
  out << "    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);\n";
  out << "    for (const auto &token : args) {\n";
  out << "        if (token == \"-h\" || token == \"--help\") {\n";
  out << "            std::cout << \"" << detail::c_escape(manifest.program) << " " << detail::c_escape(manifest.version)
      << "\\n\" << \"" << detail::c_escape(manifest.about) << "\\n\";\n";
  out << "            return 0;\n";
  out << "        }\n";
  out << "    }\n";
  out << "    auto result = cli.parse(registry.lookup(\"" << detail::c_escape(manifest.program)
      << "\")->name, args);\n";
  out << "    if (!result.ok) {\n";
  out << "        for (const auto &diag : result.diagnostics) std::cerr << diag << \"\\n\";\n";
  out << "        return 1;\n";
  out << "    }\n";
  out << "    return 0;\n";
  out << "}\n";
  return out.str();
}

// Generate a plain-text help page summarizing the manifest.
inline auto generate_help_page(const CLIManifestSpec &manifest) -> std::string {
  std::ostringstream out;
  out << manifest.program << " " << manifest.version << "\n";
  if (!manifest.about.empty()) out << manifest.about << "\n";
  out << "\nUSAGE\n    " << manifest.program << " [OPTIONS]";
  if (!manifest.commands.empty()) out << " <COMMAND>";
  out << "\n\nOPTIONS\n";
  if (manifest.arguments.empty()) {
    out << "    (none)\n";
  } else {
    for (const auto &arg : manifest.arguments) {
      std::string label;
      for (std::size_t i = 0; i < arg.names.size(); ++i) {
        if (i != 0) label += ", ";
        label += arg.names[i];
      }
      if (label.empty()) label = arg.id;
      out << "    " << label;
      if (arg.required) out << "  (required)";
      if (arg.default_value.has_value()) out << "  [default: " << *arg.default_value << "]";
      out << "\n        " << (arg.help.empty() ? std::string("(no description)") : arg.help) << "\n";
    }
  }
  if (!manifest.commands.empty()) {
    out << "\nCOMMANDS\n";
    for (const auto &command : manifest.commands) {
      out << "    " << command.name;
      if (!command.help.empty()) out << "  - " << command.help;
      out << "\n";
    }
  }
  return out.str();
}

// Generate a manpage (ROFF/nroff source) for the manifest.
inline auto generate_manpage(const CLIManifestSpec &manifest) -> std::string {
  const auto upper = [&] {
    std::string name = manifest.program;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name;
  }();
  std::ostringstream out;
  out << ".TH " << upper << " 1 \"" << manifest.version << "\" \"" << manifest.program << " Manual\"\n";
  out << ".SH NAME\n" << manifest.program;
  if (!manifest.about.empty()) out << " \\- " << manifest.about;
  out << "\n.SH SYNOPSIS\n.B " << manifest.program << "\n.RI [OPTIONS] ";
  if (!manifest.commands.empty()) out << "<COMMAND>";
  out << "\n";
  if (!manifest.about.empty()) {
    out << ".SH DESCRIPTION\n" << manifest.about << "\n";
  }
  if (!manifest.arguments.empty()) {
    out << ".SH OPTIONS\n";
    for (const auto &arg : manifest.arguments) {
      std::string label;
      for (std::size_t i = 0; i < arg.names.size(); ++i) {
        if (i != 0) label += ", ";
        label += arg.names[i];
      }
      if (label.empty()) label = arg.id;
      out << ".TP\n.BR \"" << label << "\"\n" << (arg.help.empty() ? std::string("(no description)") : arg.help) << "\n";
    }
  }
  if (!manifest.commands.empty()) {
    out << ".SH COMMANDS\n";
    for (const auto &command : manifest.commands) {
      out << ".TP\n.B " << command.name << "\n" << command.help << "\n";
    }
  }
  out << ".SH VERSION\n" << manifest.version << "\n";
  return out.str();
}

} // namespace klyspec
