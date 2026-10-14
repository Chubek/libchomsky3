#include "DSLtk.hpp"

#include <iostream>
#include <string_view>

int
main ()
{
  enum class LexemeClass
  {
    Number,
    Keyword,
    Identifier,
    Other
  };

  auto classify_by_key = [](std::string_view token) {
    if (dsl::pattern<"[0-9]+">::matches (token))
      return LexemeClass::Number;
    if (dsl::pattern<"if">::matches (token)
        || dsl::pattern<"for">::matches (token)
        || dsl::pattern<"while">::matches (token))
      return LexemeClass::Keyword;
    if (dsl::pattern<"[a-zA-Z_][a-zA-Z_0-9]*">::matches (token))
      return LexemeClass::Identifier;
    return LexemeClass::Other;
  };

  auto dispatch = dsl::match (
      dsl::when<LexemeClass::Number> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          {
            return std::string (ctx) + ":number(" + std::string (token) + ")";
          }),
      dsl::when<LexemeClass::Keyword> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          {
            return std::string (ctx) + ":keyword(" + std::string (token) + ")";
          }),
      dsl::when<LexemeClass::Identifier> (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          {
            return std::string (ctx) + ":identifier(" + std::string (token) + ")";
          }),
      dsl::otherwise (
          [] (LexemeClass, std::string_view token, std::string_view ctx)
          { return std::string (ctx) + ":other(" + std::string (token) + ")"; }));

  auto classify = [&dispatch, &classify_by_key] (std::string_view token)
  {
    return dispatch (classify_by_key (token), token, "token");
  };

  std::cout << classify ("123") << "\n";
  std::cout << classify ("while") << "\n";
  std::cout << classify ("alpha_7") << "\n";
  std::cout << classify ("@bad") << "\n";
}
