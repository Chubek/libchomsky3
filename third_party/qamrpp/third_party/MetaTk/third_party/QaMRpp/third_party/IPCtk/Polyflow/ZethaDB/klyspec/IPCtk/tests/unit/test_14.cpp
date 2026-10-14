#include <catch2/catch_test_macros.hpp>
#include "../../IPCtk.hpp"

TEST_CASE("ipctk_result_smoke_14") {
  using R = dsl::Result<int, std::string>;
  R ok = R::from_ok(14);
  REQUIRE(ok.is_ok());
  REQUIRE(ok.unwrap() == 14);
}
