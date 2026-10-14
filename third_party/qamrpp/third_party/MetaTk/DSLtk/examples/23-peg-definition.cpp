#include "DSLtk.hpp"

#include <iostream>
#include <string>

int
main ()
{
  auto grammar = dsl::create_peg_definition ();
  auto &ws = grammar.add_rule<"[ \\t\\n]+"> ([] (dsl::PEGMatch &match)
                                             { std::cout << "skip_ws:" << match.length () << "\n"; });
  ws.channel = dsl::PEGIgnoreChannel;

  auto &kw_if
      = grammar.add_rule<"if"> ([] (dsl::PEGMatch &match)
                                { std::cout << "kw:" << match.value << "\n"; });
  auto &kw_then
      = grammar.add_rule<"then"> ([] (dsl::PEGMatch &match)
                                  { std::cout << "kw:" << match.value << "\n"; });
  auto &identifier = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> ([] (
                                                                      dsl::PEGMatch &match)
                                                                    { std::cout << "id:" << match.value << "\n"; });
  auto &number = grammar.add_rule<"[0-9]+"> ([] (dsl::PEGMatch &match)
                                              { std::cout << "num:" << match.value << "\n"; });
  auto &plus = grammar.add_rule<"+"> ([] (dsl::PEGMatch &match)
                                       { std::cout << "op:" << match.value << "\n"; });
  auto &minus = grammar.add_rule<"-"> ([] (dsl::PEGMatch &match)
                                        { std::cout << "op:" << match.value << "\n"; });

  auto result = grammar.parse ("if x1 + 2 then 40");
  std::cout << (result.ok () ? "parse_ok" : "parse_failed") << "\n";
  std::cout << "offset=" << result.offset << "\n";
  std::cout << "line=" << result.line << " col=" << result.column << "\n";

  if (result.failed ())
    std::cout << result.error_message () << "\n";

  auto derived = dsl::derive_peg_definition (grammar);
  auto &hex = derived.add_rule<"0x[0-9a-fA-F]+"> (
      [] (dsl::PEGMatch &match) { std::cout << "hex:" << match.value << "\n"; });

  auto derived_result = derived.parse ("if count 0x10 then");
  std::cout << (derived_result.ok () ? "derived_ok" : "derived_failed") << "\n";
  if (derived_result.failed ())
    std::cout << derived_result.error_message () << "\n";
}
