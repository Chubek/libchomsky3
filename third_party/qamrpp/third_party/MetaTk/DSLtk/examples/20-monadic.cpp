#include "DSLtk.hpp"

#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

auto
to_maybe_int (std::string_view text)
{
  if (text.empty ())
    return dsl::Maybe<int>{};
  int value = 0;
  for (char ch : text)
    {
      if (!std::isdigit (static_cast<unsigned char> (ch)))
        return dsl::Maybe<int>{};
      value = value * 10 + (ch - '0');
    }
  return dsl::Maybe<int>{value};
}

auto
to_double_or_fail (int x)
{
  if (x == 0)
    return dsl::Maybe<double>{};
  return dsl::Maybe<double>{1.0 / x};
}

int
main ()
{
  auto parsed = to_maybe_int ("12").map ([] (int x) { return x * 4; }).filter (
      [] (int x) { return x > 10; });

  auto ratio = to_maybe_int ("4").flat_map (to_double_or_fail).map (
      [] (double x) { return x + 1.0; });
  auto failed = to_maybe_int ("n/a").flat_map (to_double_or_fail);

  std::optional<int> opt = 7;
  auto mapped = dsl::map (opt, [] (int x) { return x + 1; });
  auto filtered = dsl::filter (opt, [] (int x) { return x % 2 == 1; });

  std::cout << parsed.or_else (0) << "\n";
  std::cout << ratio.or_else (0) << "\n";
  std::cout << failed.or_else (0.0) << "\n";
  std::cout << mapped.value_or (0) << "\n";
  std::cout << filtered.value_or (0) << "\n";
}
