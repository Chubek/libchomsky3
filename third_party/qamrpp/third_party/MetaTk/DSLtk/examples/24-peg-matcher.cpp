#include "DSLtk.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int
main ()
{
  auto grammar = dsl::create_peg_definition ();
  auto &ws = grammar.add_rule<"[ \\t\\n]+"> ([] (dsl::PEGMatch &) {});
  ws.channel = dsl::PEGIgnoreChannel;

  auto &keyword = grammar.add_rule<"print"> ([] (dsl::PEGMatch &match)
                                              { std::cout << "kw:" << match.value << "\n"; });
  auto &number = grammar.add_rule<"[0-9]+"> ([] (dsl::PEGMatch &match)
                                                { std::cout << "num:" << match.value << "\n"; });
  auto &ident = grammar.add_rule<"[a-zA-Z_][a-zA-Z_0-9]*"> ([] (
                                                                     dsl::PEGMatch &match)
                                                                   { std::cout << "id:" << match.value << "\n"; });
  auto &plus = grammar.add_rule<"+"> ([] (dsl::PEGMatch &match)
                                        { std::cout << "op:" << match.value << "\n"; });

  std::string source = "print x+10 if_bad";
  auto m = dsl::peg_new_matcher (grammar, source);
  m.only ("@DEFAULT");

  while (!m.is_spent ())
    {
      m << keyword
          ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
        << number
               ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
        << ident ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]\n"; })
        << plus ([&] (dsl::PEGMatch &match) { std::cout << "[" << match.value << "]"; })
        << m.wildcard ([&] (dsl::PEGMatch &match)
                       { std::cout << "[unknown:'" << match.value << "']"; });
    }
  m.close ();

  std::cout << "\npos=" << m.position () << "\n";
}
