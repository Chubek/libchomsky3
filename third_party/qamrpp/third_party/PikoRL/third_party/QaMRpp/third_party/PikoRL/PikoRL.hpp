#ifndef PICORL_HPP
#define PICORL_HPP

#include "QaMRpp/QaMRpp.hpp"
#include "third_party/SerdeTk/SerdeTk.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace picorl
{

// The lpicorl extension that binds PicoRL features into the QaMRpp context
class PicoRLExtension : public qamrpp::Extension
{
public:
  const char *
  name () const override
  {
    return "PikoRL";
  }

  void
  register_functions (qamrpp::Context &ctx) override
  {
    ctx.register_native (
        "lpicorl_prompt",
        [] (qamrpp::Context &c, std::vector<qamrpp::ValuePtr> &args)
          {
            std::cout << (args.size () > 0 ? args[0]->string_value : "> ")
                      << std::flush;
            std::string line;
            std::getline (std::cin, line);
            auto val = std::make_shared<qamrpp::Value> ();
            val->type = qamrpp::Value::Type::STRING;
            val->string_value = line;
            return val;
          });

    ctx.register_native (
        "lpicorl_directive",
        [] (qamrpp::Context &c, std::vector<qamrpp::ValuePtr> &args)
          {
            return std::make_shared<qamrpp::Value> ();
          });

    ctx.register_native (
        "lpicorl_plugin",
        [] (qamrpp::Context &c, std::vector<qamrpp::ValuePtr> &args)
          {
            if (!args.empty () && args[0]->type == qamrpp::Value::Type::STRING)
              {
                bool ok = c.load_library (args[0]->string_value);
                auto res = std::make_shared<qamrpp::Value> ();
                res->type = qamrpp::Value::Type::BOOL;
                res->bool_value = ok;
                return res;
              }
            return std::make_shared<qamrpp::Value> ();
          });
  }
};

// The Main REPL Toolkit Class
class REPL
{
private:
  qamrpp::Context ctx;
  bool is_running;

public:
  REPL () : is_running (false)
  {
    // Load QaMRpp Standard libraries
    qamrpp::stdlib::load_core (ctx);
    qamrpp::stdlib::load_string (ctx);
    qamrpp::stdlib::load_table (ctx);
    qamrpp::stdlib::load_math (ctx);
    qamrpp::stdlib::load_io (ctx);
    qamrpp::stdlib::load_os (ctx);
    qamrpp::stdlib::load_package (ctx);

    // Add the PicoRL Extension
    ctx.add_extension (std::make_unique<PicoRLExtension> ());

    // Bootstrap the lpicorl table in Lua space
    ctx.run (
        "local function __lpikorl_nil() return nil end "
        "lpicorl = {"
        " prompt = lpicorl_prompt or __lpikorl_nil,"
        " plugin = lpicorl_plugin,"
        " syntax = lpicorl_syntax or __lpikorl_nil,"
        " directive = lpicorl_directive or __lpikorl_nil,"
        " completion = lpicorl_completion or __lpikorl_nil,"
        " help = lpicorl_help or __lpikorl_nil,"
        " history = lpicorl_history or __lpikorl_nil,"
        " menu = lpicorl_menu or __lpikorl_nil,"
        " page = lpicorl_page or __lpikorl_nil,"
        " idl = lpicorl_idl or __lpikorl_nil,"
        " theme = lpicorl_theme or __lpikorl_nil,"
        " colors = lpicorl_colors or __lpikorl_nil,"
        " macros = lpicorl_macros or __lpikorl_nil,"
        " web = lpicorl_web or __lpikorl_nil,"
        " sandboxing = lpicorl_sandboxing or __lpikorl_nil"
        " }");

    (void)load_example_bundle ("examples/PythonRL");
    (void)load_example_bundle ("examples/RubyRL");
  }

  void
  run ()
  {
    is_running = true;
    std::cout << "PicoRL - Header-Only REPL Toolkit\n";
    std::cout << "Powered by QaMRpp Lua\n";

    while (is_running)
      {
        std::string input;
        bool got_line = false;

        try
          {
            auto prompt_result
                = ctx.run ("if lpicorl and lpicorl.prompt "
                           "then return lpicorl.prompt('PicoRL> ') "
                           "end return nil");
            if (prompt_result
                && prompt_result->type == qamrpp::Value::Type::STRING)
              {
                input = prompt_result->string_value;
                got_line = true;
              }
          }
        catch (const std::exception &)
          {
          }

        if (!got_line)
          {
            std::cout << "PicoRL> " << std::flush;
            if (!std::getline (std::cin, input))
              {
                break;
              }
          }

        if (input == "exit" || input == "quit")
          {
            is_running = false;
            break;
          }

        try
          {
            ctx.run (input);
          }
        catch (const std::exception &e)
          {
            std::cerr << "Error: " << e.what () << "\n";
          }
      }
  }

  bool
  load_example_bundle (const std::string &bundle_dir)
  {
    namespace fs = std::filesystem;
    const fs::path base (bundle_dir);
    const fs::path manifest = base / "MANIFEST.json";
    if (!fs::exists (manifest))
      {
        return false;
      }

    try
      {
        serdetk::register_builtin_formats ();
        const auto doc = serdetk::builtins::json ().load_file (manifest);
        if (!doc.root.is_object ())
          {
            return false;
          }

        const auto &root = doc.root.as_object ();
        auto run_file = [&] (const std::string &relpath) -> bool
        {
          const fs::path full = base / relpath;
          if (!fs::exists (full))
            {
              return false;
            }
          std::ifstream in (full);
          if (!in)
            {
              return false;
            }
          std::string source ((std::istreambuf_iterator<char> (in)),
                              std::istreambuf_iterator<char> ());
          ctx.run (source);
          return true;
        };

        if (root.contains ("lua"))
          {
            const auto &lua_list = root.at ("lua");
            if (lua_list.is_array ())
              {
                for (const auto &entry : lua_list.as_array ().items)
                  {
                    if (entry.is_string ())
                      {
                        (void)run_file (entry.as_string ());
                      }
                    else if (entry.is_object ())
                      {
                        const auto &obj = entry.as_object ();
                        bool required = false;
                        if (obj.contains ("required")
                            && obj.at ("required").is_bool ())
                          {
                            required = std::get<bool> (
                                obj.at ("required").data);
                          }

                        if (obj.contains ("path")
                            && obj.at ("path").is_string ())
                          {
                            const bool loaded
                                = run_file (obj.at ("path").as_string ());
                            if (!loaded && required)
                              {
                                return false;
                              }
                          }
                      }
                  }
              }
          }
      }
    catch (const std::exception &)
      {
        return false;
      }

    return true;
  }

  // Allow host language to inject its own APIs/Globals into the REPL
  template <typename F>
  void
  bind_api (const std::string &name, F function)
  {
    ctx.register_native (name, function);
  }
};

} // namespace picorl

#endif // PICORL_HPP
