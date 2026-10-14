#include "DSLtk.hpp"

#include <cctype>
#include <iostream>

auto
to_span_parser (const auto &inner)
{
  return dsl::parser (
      [inner] (dsl::ParsecInput &in) -> dsl::ExpectedResult<dsl::PEGMatch>
      {
        auto begin = in.pos;
        auto r = inner (in);
        if (!r)
          return dsl::ExpectedResult<dsl::PEGMatch>::failure (
              r.error.pos, r.error.kind, r.error.expected);
        dsl::PEGMatch m;
        m.begin = begin;
        m.end = in.pos;
        m.value = in.get_span (begin);
        return m;
      });
}

void
print_parse_result (std::string_view label, const auto &result)
{
  if (!result.value)
    {
      std::cout << label << " = fail\n";
      return;
    }
  std::cout << label << " = " << result.value->value << "\n";
}

int
main ()
{
  auto digit = dsl::satisfy (
      [] (char c) { return std::isdigit (static_cast<unsigned char> (c)); },
      "digit");
  auto alpha = dsl::satisfy (
      [] (char c) { return std::isalpha (static_cast<unsigned char> (c)); },
      "alpha");
  auto alnum = dsl::satisfy (
      [] (char c) { return std::isalnum (static_cast<unsigned char> (c)); },
      "alnum");

  auto integer_core = dsl::peg_seq (
      dsl::peg_opt (dsl::ch ('+') | dsl::ch ('-')),
      dsl::peg_many1 (digit));
  auto identifier_core = dsl::peg_seq (
      dsl::peg_and (alpha), dsl::peg_many1 (alnum));

  auto integer = to_span_parser (integer_core);
  auto identifier = to_span_parser (identifier_core);
  auto token = dsl::peg_choice (integer, identifier);

  auto signed_only = dsl::peg_seq (dsl::peg_not (dsl::ch ('-')),
                                   dsl::peg_many1 (digit));
  auto signed_only_value = to_span_parser (signed_only);

  print_parse_result ("integer:12", dsl::run_parser (integer, "12"));
  print_parse_result ("integer:-9", dsl::run_parser (integer, "-9"));
  print_parse_result ("identifier:abc", dsl::run_parser (identifier, "abc"));
  print_parse_result ("number_or_identifier:xyz", dsl::run_parser (token, "xyz"));
  print_parse_result ("number_or_identifier:99", dsl::run_parser (token, "99"));

  auto plus_ok = dsl::run_parser (signed_only_value, "8");
  auto plus_fail = dsl::run_parser (signed_only_value, "-8");
  std::cout << "signed_no_minus:8 => "
            << (plus_ok.value ? "ok" : "fail") << "\n";
  std::cout << "signed_no_minus:-8 => "
            << (plus_fail.value ? "ok" : "fail") << "\n";

  (void) signed_only;
}
