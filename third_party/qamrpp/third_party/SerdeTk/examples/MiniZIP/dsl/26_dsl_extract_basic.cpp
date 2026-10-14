#include "MiniZIP.hpp"

int main() {
    auto plan =
        minizip::dsl::extract("dist/dsl-basic.mz")
        | minizip::dsl::to("out/dsl-basic")
        | minizip::dsl::on_conflict(minizip::api::overwrite::Replace);

    return plan.extract_all().ok() ? 0 : 1;
}
