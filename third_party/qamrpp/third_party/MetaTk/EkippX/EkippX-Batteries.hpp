/**
 * @file EkippX-Batteries.hpp
 * @brief Standard EkippX directives, functions, expanders, and conditionals.
 *
 * The batteries layer installs portable text, numeric, path, CSV, conditional,
 * include, symbol, and diagnostic helpers into a caller-supplied Context. It is
 * the canonical baseline used by examples, tests, the CLI, and the REPL. Hosts
 * that require a smaller trusted surface may register individual functions
 * manually instead of calling register_all().
 *
 * @section ekippx_batteries_policy Policy
 * Batteries obey CompileOptions limits and include policy. Filesystem-facing
 * helpers reject traversal outside configured roots when sandboxing is active.
 */
#pragma once

#include "EkippX-Plugin.hpp"
#include "plugins/DateTimePlugin.hpp"
#include "plugins/EnvironmentPlugin.hpp"
#include "plugins/FilesystemPlugin.hpp"
#include "plugins/RandomPlugin.hpp"
#include "plugins/TextPlugin.hpp"

#include <chrono>
#include <cstdlib>
#include <random>
#include <thread>

namespace ekippx::batteries {

inline std::string join_arguments(const std::vector<std::string>& arguments, std::string_view separator = " ") {
  std::ostringstream stream;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    if (index != 0) stream << separator;
    stream << arguments[index];
  }
  return stream.str();
}

inline std::string csv_escape(std::string_view input) {
  const bool needs_quotes = input.find_first_of(",\"") != std::string_view::npos;
  std::string escaped(input);
  escaped = detail::replace_all(std::move(escaped), "\"", "\"\"");
  return needs_quotes ? "\"" + escaped + "\"" : escaped;
}

inline long long parse_integer_arg(const Invocation& inv, std::size_t index, long long minimum, long long maximum) {
  try {
    std::size_t parsed = 0;
    const auto value = std::stoll(inv.args.at(index), &parsed);
    if (parsed != inv.args.at(index).size()) throw FunctionError("numeric argument contains trailing characters");
    if (value < minimum || value > maximum) throw FunctionError("numeric argument outside allowed range");
    return value;
  } catch (const std::exception& error) {
    throw FunctionError(inv.callee + " argument " + std::to_string(index + 1) + " expects integer in range [" + std::to_string(minimum) + ", " + std::to_string(maximum) + "]: " + error.what());
  }
}

inline double parse_double_arg(const Invocation& inv, std::size_t index) {
  try {
    std::size_t parsed = 0;
    const auto value = std::stod(inv.args.at(index), &parsed);
    if (parsed != inv.args.at(index).size()) throw FunctionError("numeric argument contains trailing characters");
    return value;
  } catch (const std::exception& error) {
    throw FunctionError(inv.callee + " argument " + std::to_string(index + 1) + " expects number: " + error.what());
  }
}

inline void require_identifier_arg(const Invocation& inv, std::size_t index, std::string_view role) {
  if (!detail::is_identifier(inv.args.at(index))) throw DirectiveError(inv.callee + " argument " + std::to_string(index + 1) + " expects " + std::string(role) + " identifier");
}

inline void require_env_name(const Invocation& inv, std::size_t index) {
  const auto& name = inv.args.at(index);
  const bool ok = !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '_';
  });
  if (!ok) throw FunctionError(inv.callee + " argument " + std::to_string(index + 1) + " expects environment variable name");
}

inline void install_default_directives(Context& context) {
  context.register_directive({.name = "emit", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    ctx.emit(join_arguments(inv.args, ctx.config().lex.sigils.all_args_separator));
  });
  context.register_directive({.name = "emitln", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    ctx.emit_line(join_arguments(inv.args, ctx.config().lex.sigils.all_args_separator));
  });
  context.register_directive({.name = "define", .min_arity = 2, .max_arity = 2, .allow_raw_arguments = true}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "macro");
    ctx.macros().define_object(inv.args.at(0), inv.args.at(1));
    ctx.symbols().register_macro_name(inv.args.at(0));
  });
  context.register_directive({.name = "deflit", .min_arity = 2, .max_arity = 2, .allow_raw_arguments = true}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "macro");
    ctx.macros().define_literal(inv.args.at(0), inv.args.at(1));
    ctx.symbols().register_macro_name(inv.args.at(0));
  });
  context.register_directive({.name = "undef", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    ctx.macros().undefine(inv.args.at(0));
    ctx.symbols().undefine(inv.args.at(0));
  });
  context.register_directive({.name = "defsym", .min_arity = 2, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "symbol");
    ctx.define_symbol(inv.args.at(0), inv.args.at(1));
  });
  context.register_directive({.name = "alias", .min_arity = 2, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    ctx.symbols().alias(inv.args.at(0), inv.args.at(1));
  });
  context.register_directive({.name = "unalias", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    ctx.symbols().unalias(inv.args.at(0));
  });
  context.register_directive({.name = "include", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    if (ctx.config().runtime.include_policy == IncludePolicy::deny) throw IncludeError("include policy denies file inclusion");
    Path requested(inv.args.at(0));
    if (requested.empty() || detail::path_has_traversal(requested)) throw IncludeError("include path must not be empty or contain traversal");
    Path resolved = requested;
    if (requested.is_relative()) {
      bool found = false;
      for (const auto& include_path : ctx.config().environment.include_paths) {
        const Path candidate = include_path / requested;
        if (std::filesystem::exists(candidate)) {
          resolved = candidate;
          found = true;
          break;
        }
      }
      if (!found) resolved = ctx.config().environment.working_directory / requested;
    }
    std::ifstream input(resolved);
    if (!input) throw IncludeError("failed to read include file: " + resolved.string());
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    ctx.emit(ctx.expand_text(text, resolved.string()));
  });
  context.register_directive({.name = "tracepoint", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    ctx.trace_marker(inv.args.at(0));
  });
  context.register_directive({.name = "config", .min_arity = 2, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    const auto key = detail::lower_copy(inv.args.at(0));
    const auto& value = inv.args.at(1);
    if (key == "strictmode" || key == "strict") ctx.config().runtime.strict_mode = detail::truthy(value);
    else if (key == "traceenabled" || key == "trace") ctx.config().runtime.trace_enabled = detail::truthy(value);
    else if (key == "separator") ctx.config().lex.sigils.separator = value;
    else if (key == "allargsseparator") ctx.config().lex.sigils.all_args_separator = value;
    else ctx.set_metadata(inv.args.at(0), value);
  });
  context.register_directive({.name = "if", .min_arity = 2, .max_arity = 3}, [](Context& ctx, const Invocation& inv) {
    const bool condition = ctx.evaluate_conditional(inv.args.at(0), inv.range);
    if (condition) ctx.emit(ctx.expand_text(inv.args.at(1)));
    else if (inv.args.size() >= 3) ctx.emit(ctx.expand_text(inv.args.at(2)));
  });
  context.register_directive({.name = "counter", .min_arity = 1, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "counter");
    const auto value = inv.args.size() == 2 ? parse_integer_arg(inv, 1, -1000000000LL, 1000000000LL) : 0LL;
    ctx.set_counter(inv.args.at(0), value);
  });
  context.register_directive({.name = "inc", .min_arity = 1, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "counter");
    ctx.inc_counter(inv.args.at(0), inv.args.size() == 2 ? parse_integer_arg(inv, 1, -1000000000LL, 1000000000LL) : 1LL);
  });
  context.register_directive({.name = "dec", .min_arity = 1, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "counter");
    ctx.dec_counter(inv.args.at(0), inv.args.size() == 2 ? parse_integer_arg(inv, 1, -1000000000LL, 1000000000LL) : 1LL);
  });
  context.register_directive({.name = "plugin", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    require_identifier_arg(inv, 0, "plugin");
    if (!ctx.activate_plugin(inv.args.at(0))) throw DirectiveError("unknown plugin: " + inv.args.at(0));
  });
  context.register_directive({.name = "pluginpath", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    ctx.add_include_path(inv.args.at(0));
  });
  context.register_directive({.name = "requireplugin", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    if (!ctx.is_plugin_loaded(inv.args.at(0))) throw DirectiveError("required plugin is not loaded: " + inv.args.at(0));
  });
  context.register_directive({.name = "warning", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    ctx.add_diagnostic(DiagnosticLevel::warning, "warning", join_arguments(inv.args), inv.range);
  });
  context.register_directive({.name = "notice", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    ctx.add_diagnostic(DiagnosticLevel::note, "notice", join_arguments(inv.args), inv.range);
  });
}

inline void install_default_functions(Context& context) {
  context.register_function({.name = "echo", .min_arity = 0, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    return join_arguments(inv.args);
  });
  context.register_function({.name = "upper", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    auto text = inv.args.at(0);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return text;
  });
  context.register_function({.name = "lower", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    auto text = inv.args.at(0);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
  });
  context.register_function({.name = "trim", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    return detail::trim(inv.args.at(0));
  });
  context.register_function({.name = "title", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    auto text = detail::lower_copy(inv.args.at(0));
    bool make_upper = true;
    for (char& ch : text) {
      if (std::isspace(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
        make_upper = true;
        continue;
      }
      if (make_upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      make_upper = false;
    }
    return text;
  });
  context.register_function({.name = "replace", .min_arity = 3, .max_arity = 3}, [](Context&, const Invocation& inv) {
    return detail::replace_all(inv.args.at(0), inv.args.at(1), inv.args.at(2));
  });
  context.register_function({.name = "join", .min_arity = 1, .max_arity = std::nullopt}, [](Context& ctx, const Invocation& inv) {
    if (inv.args.size() == 1) return inv.args.front();
    return join_arguments({inv.args.begin() + 1, inv.args.end()}, inv.args.front());
  });
  context.register_function({.name = "count", .min_arity = 0, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    return std::to_string(inv.args.size());
  });
  context.register_function({.name = "first", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    return inv.args.front();
  });
  context.register_function({.name = "last", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    return inv.args.back();
  });
  context.register_function({.name = "repeatText", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    const auto count = parse_integer_arg(inv, 1, 0, 100000);
    std::string out;
    for (long long index = 0; index < count; ++index) out += inv.args.at(0);
    return out;
  });
  context.register_function({.name = "padLeft", .min_arity = 2, .max_arity = 3}, [](Context&, const Invocation& inv) {
    const auto width = static_cast<std::size_t>(parse_integer_arg(inv, 1, 0, 100000));
    const auto fill = inv.args.size() == 3 && !inv.args.at(2).empty() ? inv.args.at(2).front() : ' ';
    if (inv.args.at(0).size() >= width) return inv.args.at(0);
    return std::string(width - inv.args.at(0).size(), fill) + inv.args.at(0);
  });
  context.register_function({.name = "padRight", .min_arity = 2, .max_arity = 3}, [](Context&, const Invocation& inv) {
    const auto width = static_cast<std::size_t>(parse_integer_arg(inv, 1, 0, 100000));
    const auto fill = inv.args.size() == 3 && !inv.args.at(2).empty() ? inv.args.at(2).front() : ' ';
    if (inv.args.at(0).size() >= width) return inv.args.at(0);
    return inv.args.at(0) + std::string(width - inv.args.at(0).size(), fill);
  });
  context.register_function({.name = "center", .min_arity = 2, .max_arity = 3}, [](Context&, const Invocation& inv) {
    const auto width = static_cast<std::size_t>(parse_integer_arg(inv, 1, 0, 100000));
    const auto fill = inv.args.size() == 3 && !inv.args.at(2).empty() ? inv.args.at(2).front() : ' ';
    if (inv.args.at(0).size() >= width) return inv.args.at(0);
    const auto total = width - inv.args.at(0).size();
    const auto left = total / 2;
    return std::string(left, fill) + inv.args.at(0) + std::string(total - left, fill);
  });
  context.register_function({.name = "add", .min_arity = 2, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    double value = 0.0;
    for (std::size_t index = 0; index < inv.args.size(); ++index) value += parse_double_arg(inv, index);
    std::ostringstream stream;
    stream << value;
    return stream.str();
  });
  context.register_function({.name = "abs", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    std::ostringstream stream;
    stream << std::abs(parse_double_arg(inv, 0));
    return stream.str();
  });
  context.register_function({.name = "min", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    double value = parse_double_arg(inv, 0);
    for (std::size_t index = 0; index < inv.args.size(); ++index) value = std::min(value, parse_double_arg(inv, index));
    std::ostringstream stream;
    stream << value;
    return stream.str();
  });
  context.register_function({.name = "max", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    double value = parse_double_arg(inv, 0);
    for (std::size_t index = 0; index < inv.args.size(); ++index) value = std::max(value, parse_double_arg(inv, index));
    std::ostringstream stream;
    stream << value;
    return stream.str();
  });
  context.register_function({.name = "clamp", .min_arity = 3, .max_arity = 3}, [](Context&, const Invocation& inv) {
    const auto value = parse_double_arg(inv, 0);
    const auto lower = parse_double_arg(inv, 1);
    const auto upper = parse_double_arg(inv, 2);
    if (lower > upper) throw FunctionError("clamp lower bound must not exceed upper bound");
    std::ostringstream stream;
    stream << std::clamp(value, lower, upper);
    return stream.str();
  });
  context.register_function({.name = "uuid", .min_arity = 0, .max_arity = 0}, [](Context&, const Invocation&) {
    static thread_local std::mt19937_64 generator{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> distribution;
    std::ostringstream stream;
    for (int index = 0; index < 4; ++index) {
      if (index != 0) stream << "-";
      stream << std::hex << std::setw(4 + (index % 2) * 4) << std::setfill('0') << (distribution(generator) & 0xffffffffu);
    }
    return stream.str();
  });
  context.register_function({.name = "timestamp", .min_arity = 0, .max_arity = 1}, [](Context&, const Invocation& inv) {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    if (!inv.args.empty() && detail::lower_copy(inv.args.at(0)) == "unix") return std::to_string(seconds);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &seconds);
#else
    localtime_r(&seconds, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return stream.str();
  });
  context.register_function({.name = "hash", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    std::hash<std::string> hasher;
    std::ostringstream stream;
    stream << inv.args.at(0) << ":" << std::hex << hasher(inv.args.at(1));
    return stream.str();
  });
  context.register_function({.name = "checksum", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    std::uint64_t sum = 0;
    for (unsigned char ch : inv.args.at(0)) sum += ch;
    return std::to_string(sum);
  });
  context.register_function({.name = "jsonEncode", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    std::string escaped = detail::replace_all(inv.args.at(0), "\\", "\\\\");
    escaped = detail::replace_all(std::move(escaped), "\"", "\\\"");
    return "\"" + escaped + "\"";
  });
  context.register_function({.name = "jsonDecode", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    return detail::quote_unwrap(inv.args.at(0));
  });
  context.register_function({.name = "csvsplit", .min_arity = 1, .max_arity = 1}, [](Context&, const Invocation& inv) {
    auto parts = detail::split_arguments(inv.args.at(0));
    return join_arguments(parts, "|");
  });
  context.register_function({.name = "csvjoin", .min_arity = 1, .max_arity = std::nullopt}, [](Context&, const Invocation& inv) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < inv.args.size(); ++index) {
      if (index != 0) stream << ",";
      stream << csv_escape(inv.args.at(index));
    }
    return stream.str();
  });
  context.register_function({.name = "envget", .min_arity = 1, .max_arity = 2}, [](Context& ctx, const Invocation& inv) {
    require_env_name(inv, 0);
    if (const auto override_it = ctx.config().environment.env_overrides.find(inv.args.at(0)); override_it != ctx.config().environment.env_overrides.end()) return override_it->second;
    if (const char* value = std::getenv(inv.args.at(0).c_str())) return std::string(value);
    return inv.args.size() == 2 ? inv.args.at(1) : std::string{};
  });
  context.register_function({.name = "envhas", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) -> std::string {
    require_env_name(inv, 0);
    if (ctx.config().environment.env_overrides.contains(inv.args.at(0))) return std::string("true");
    return std::getenv(inv.args.at(0).c_str()) ? std::string("true") : std::string("false");
  });
  context.register_function({.name = "counterValue", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    if (!detail::is_identifier(inv.args.at(0))) throw FunctionError("counterValue argument 1 expects counter identifier");
    return std::to_string(ctx.counter(inv.args.at(0)));
  });
  context.register_function({.name = "sleep", .min_arity = 1, .max_arity = 1, .pure = false}, [](Context& ctx, const Invocation& inv) {
    if (ctx.config().runtime.shell_policy == ShellPolicy::deny && ctx.config().runtime.strict_mode) throw FunctionError("sleep is disabled in strict mode");
    std::this_thread::sleep_for(std::chrono::milliseconds(parse_integer_arg(inv, 0, 0, 5000)));
    return std::string{};
  });
}

inline void install_default_expanders(Context& context) {
  context.register_expander({.name = "CONFIG", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) -> std::string {
    const auto key = detail::lower_copy(inv.args.at(0));
    if (key == "strictmode" || key == "strict") return ctx.config().runtime.strict_mode ? std::string("true") : std::string("false");
    if (key == "traceenabled" || key == "trace") return ctx.config().runtime.trace_enabled ? std::string("true") : std::string("false");
    if (key == "separator") return ctx.config().lex.sigils.separator;
    if (const auto found = ctx.config().metadata.find(inv.args.at(0)); found != ctx.config().metadata.end()) return found->second.to_string();
    return std::string{};
  });
  context.register_expander({.name = "META", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    if (const auto found = ctx.metadata().find(inv.args.at(0)); found != ctx.metadata().end()) return found->second.to_string();
    return std::string{};
  });
  context.register_expander({.name = "RANDOM", .min_arity = 0, .max_arity = 2}, [](Context&, const Invocation& inv) {
    static thread_local std::mt19937 generator{std::random_device{}()};
    int low = 0;
    int high = std::numeric_limits<int>::max();
    if (inv.args.size() == 2) {
      low = static_cast<int>(parse_integer_arg(inv, 0, -1000000000LL, 1000000000LL));
      high = static_cast<int>(parse_integer_arg(inv, 1, -1000000000LL, 1000000000LL));
    }
    if (low > high) throw ExpanderError("RANDOM lower bound must not exceed upper bound");
    std::uniform_int_distribution<int> distribution(low, high);
    return std::to_string(distribution(generator));
  });
  context.register_expander({.name = "UUID", .min_arity = 0, .max_arity = 0}, [](Context& ctx, const Invocation& inv) {
    return ctx.functions().handler("uuid") ? (*ctx.functions().handler("uuid"))(ctx, Invocation{"uuid", {}, inv.range}) : std::string{};
  });
  context.register_expander({.name = "COUNTER", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    return std::to_string(ctx.counter(inv.args.at(0)));
  });
  context.register_expander({.name = "PLUGIN", .min_arity = 2, .max_arity = 2}, [](Context& ctx, const Invocation& inv) -> std::string {
    if (detail::lower_copy(inv.args.at(1)) == "enabled") return ctx.is_plugin_loaded(inv.args.at(0)) ? std::string("true") : std::string("false");
    return std::string{};
  });
  context.register_expander({.name = "DATE", .min_arity = 0, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    Invocation call{"timestamp", inv.args, inv.range};
    return (*ctx.functions().handler("timestamp"))(ctx, call);
  });
}

inline void install_default_conditionals(Context& context) {
  context.register_conditional({.name = "IfEq?", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    return inv.args.at(0) == inv.args.at(1);
  });
  context.register_conditional({.name = "IfContains?", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    return inv.args.at(0).find(inv.args.at(1)) != std::string::npos;
  });
  context.register_conditional({.name = "IfStartsWith?", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    return inv.args.at(0).rfind(inv.args.at(1), 0) == 0;
  });
  context.register_conditional({.name = "IfEndsWith?", .min_arity = 2, .max_arity = 2}, [](Context&, const Invocation& inv) {
    return inv.args.at(0).size() >= inv.args.at(1).size() &&
           inv.args.at(0).compare(inv.args.at(0).size() - inv.args.at(1).size(), inv.args.at(1).size(), inv.args.at(1)) == 0;
  });
  context.register_conditional({.name = "IfPluginLoaded?", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    return ctx.is_plugin_loaded(inv.args.at(0));
  });
  context.register_conditional({.name = "IfConfig?", .min_arity = 1, .max_arity = 1}, [](Context& ctx, const Invocation& inv) {
    const auto key = detail::lower_copy(inv.args.at(0));
    return key == "strictmode" ? ctx.config().runtime.strict_mode
                               : ctx.config().metadata.contains(inv.args.at(0));
  });
}

inline void register_all(Context& context) {
  install_default_directives(context);
  install_default_functions(context);
  install_default_expanders(context);
  install_default_conditionals(context);
  const auto install_static = [&](plugin::StaticPlugin plugin) {
    const auto plugin_name = plugin.descriptor.info.name;
    context.register_plugin_loader(plugin_name, [plugin = std::move(plugin)](Context& ctx) {
      static plugin::AtomicRegistry atomic_registry;
      static plugin::HookRegistry hook_registry;
      auto plugin_context = plugin::make_plugin_context(plugin::host_info(ctx), plugin.descriptor.info.name);
      plugin::Registrar registrar(ctx, plugin_context, atomic_registry, hook_registry);
      const auto report = plugin.compatibility ? (*plugin.compatibility)(plugin_context.host_info)
                                               : plugin::check_plugin_compatibility(plugin.descriptor, plugin_context.host_info);
      if (report.result != plugin::CompatibilityResult::compatible) throw plugin::PluginCapabilityError(report.message);
      plugin.initialize(registrar, plugin_context);
    });
  };
  install_static(plugins::make_filesystem_plugin());
  install_static(plugins::make_environment_plugin());
  install_static(plugins::make_datetime_plugin());
  install_static(plugins::make_random_plugin());
  install_static(plugins::make_text_plugin());
}

inline Context batteries_context() {
  auto context = default_context();
  register_all(context);
  return context;
}

inline plugin::StaticPlugin make_batteries_plugin(std::string_view plugin_name = "ekippx.batteries") {
  plugin::PluginDescriptor descriptor;
  descriptor.info.name = std::string(plugin_name);
  descriptor.info.display_name = "EkippX Batteries";
  descriptor.info.description = "Registers the batteries-included EkippX builtins";
  descriptor.info.author = "EkippX";
  descriptor.info.kind = plugin::PluginKind::builtin_plugin;
  return plugin::make_static_plugin(
      std::move(descriptor),
      [](plugin::Registrar& registrar, plugin::PluginContext&) { register_all(registrar.host()); });
}

}  // namespace ekippx::batteries
