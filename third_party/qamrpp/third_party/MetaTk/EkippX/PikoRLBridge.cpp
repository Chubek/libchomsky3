/**
 * @file PikoRLBridge.cpp
 * @brief PikoRL integration layer for the EkippX interactive REPL.
 *
 * The bridge exposes expansion, trace inspection, and symbol-table inspection as
 * host callbacks while preserving the same Context used by batch execution.
 */
#include "EkippX-Batteries.hpp"

#include "PikoRL.hpp"

#include <iostream>
#include <memory>

namespace qamrpp::stdlib {

inline void load_core(Context& ctx) {
  (void)ctx.load_library_named("core");
}

inline void load_string(Context& ctx) {
  (void)ctx.load_library_named("string");
}

inline void load_table(Context& ctx) {
  (void)ctx.load_library_named("table");
}

inline void load_math(Context& ctx) {
  (void)ctx.load_library_named("math");
}

inline void load_io(Context& ctx) {
  (void)ctx.load_library_named("io");
}

inline void load_os(Context& ctx) {
  (void)ctx.load_library_named("os");
}

inline void load_package(Context& ctx) {
  (void)ctx.load_library_named("package");
}

}  // namespace qamrpp::stdlib

namespace ekippx::cli {

int run_pikorl_bridge() {
  try {
    auto context = std::make_shared<ekippx::Context>(ekippx::batteries::batteries_context());
    picorl::REPL repl;

    repl.bind_api("ekippx_expand", [context](qamrpp::Context&, std::vector<qamrpp::ValuePtr>& args) {
      auto result = std::make_shared<qamrpp::Value>();
      result->type = qamrpp::Value::Type::STRING;
      if (args.empty() || args.front()->type != qamrpp::Value::Type::STRING) {
        result->string_value = "error: ekippx_expand expects one string";
        return result;
      }
      const auto expanded = context->expand(ExpandRequest{"<repl>", args.front()->string_value, std::nullopt});
      result->string_value = expanded.success ? expanded.output : (!expanded.diagnostics.empty() ? expanded.diagnostics.back().message : "expansion failed");
      return result;
    });

    repl.bind_api("ekippx_trace", [context](qamrpp::Context&, std::vector<qamrpp::ValuePtr>&) {
      auto result = std::make_shared<qamrpp::Value>();
      result->type = qamrpp::Value::Type::STRING;
      result->string_value = serdetk::builtins::json().dump_string(context->trace_document());
      return result;
    });

    repl.bind_api("ekippx_symbols", [context](qamrpp::Context&, std::vector<qamrpp::ValuePtr>&) {
      auto result = std::make_shared<qamrpp::Value>();
      result->type = qamrpp::Value::Type::STRING;
      result->string_value = serdetk::builtins::json().dump_string(context->symbol_document());
      return result;
    });

    repl.run();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "PikoRL bridge error: " << error.what() << "\n";
    return 1;
  }
}

}  // namespace ekippx::cli
