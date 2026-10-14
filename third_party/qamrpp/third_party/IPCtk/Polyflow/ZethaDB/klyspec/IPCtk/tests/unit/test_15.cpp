#include <catch2/catch_test_macros.hpp>
#include "../../IPCtk.hpp"

TEST_CASE("ipctk_result_smoke_15") {
  using R = dsl::Result<int, std::string>;
  R ok = R::from_ok(15);
  REQUIRE(ok.is_ok());
  REQUIRE(ok.unwrap() == 15);
}
