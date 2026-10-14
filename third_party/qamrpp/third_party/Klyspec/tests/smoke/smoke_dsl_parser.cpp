#include "Klyspec.hpp"

#include <cassert>

int main() {
  using namespace klyspec;

  const char* good = R"(
param "b/book="
{
    help-string
    {
        Book option
    };
};
command "build"
{
};
pre-evaluate
{
    exec = "/bin/bash";
    sanitize = "stdlib/shell.sh";
};
)";
  auto ok = parse_klytmk(good);
  assert(ok.ok);
  assert(ok.ast.has_value());
  assert(ok.ast->nodes.size() == 3);

  const char* bad = R"(command "build" { )";
  auto fail = parse_klytmk(bad);
  assert(!fail.ok);
  assert(!fail.diagnostics.empty());

  return 0;
}
