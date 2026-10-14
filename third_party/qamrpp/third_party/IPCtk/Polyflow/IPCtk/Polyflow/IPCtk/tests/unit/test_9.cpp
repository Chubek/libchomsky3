#include <catch2/catch_test_macros.hpp>
#include "../../IPCtk.hpp"

TEST_CASE("ipctk_result_smoke_9") {
  using R = dsl::Result<int, std::string>;
  R ok = R::from_ok(9);
  REQUIRE(ok.is_ok());
  REQUIRE(ok.unwrap() == 9);
}
