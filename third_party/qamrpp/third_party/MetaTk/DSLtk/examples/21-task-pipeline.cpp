#include "DSLtk.hpp"

#include <cctype>
#include <iostream>

int
main ()
{
  using Result = dsl::Result<std::string, std::string>;

  auto read = dsl::Task{
      "read",
      [] (dsl::TaskState &state)
      {
        state.results["source"] = Result::from_ok ("18");
        return Result::from_ok ("read");
      }};

  auto parse = dsl::Task{
      "parse",
      [] (dsl::TaskState &state)
      {
        auto it = state.results.find ("source");
        if (it == state.results.end () || it->second.is_err ())
          return Result::from_err ("missing source");
        const auto &raw = it->second.unwrap ();
        return raw.empty () ? Result::from_err ("empty source")
                           : Result::from_ok (raw);
      }};

  auto inspect = dsl::Task{
      "inspect",
      [] (dsl::TaskState &state)
      {
        auto it = state.results.find ("parse");
        if (it == state.results.end () || it->second.is_err ())
          return Result::from_err ("nothing to inspect");
        auto raw = it->second.unwrap ();
        for (char ch : raw)
          if (!std::isdigit (static_cast<unsigned char> (ch)))
            return Result::from_err ("source must be digits");
        auto value = std::stoi (raw);
        auto parity = (value % 2 == 0) ? "even" : "odd";
        return Result::from_ok (parity);
      }};

  auto chain = read | parse | inspect;
  dsl::TaskState state{};
  dsl::run (state, chain);

  for (const auto &entry : state.results)
    {
      std::cout << entry.first << " -> ";
      std::cout << (entry.second.is_ok () ? entry.second.unwrap_or ("?") : entry.second.unwrap_or ("?"))
                << "\n";
    }

  dsl::Task normalize{ "normalize", [] (dsl::TaskState &) {
                        return Result::from_ok ("normalized");
                      } };
  dsl::upon (
      state,
      [&state]
      { return state.results.count ("inspect")
                 && state.results.at ("inspect").is_ok ()
               ? state.results.at ("inspect").unwrap () == "even"
               : false; },
      normalize);

  std::cout << "normalize="
            << (state.results.count ("normalize")
                   ? state.results["normalize"].unwrap_or (std::string ("skipped"))
                   : std::string ("skipped"))
            << "\n";
}
