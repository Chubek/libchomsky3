#include "MiniZIP.hpp"

int main() {
    auto plan =
        minizip::dsl::archive("dsl-basic.mz")
        | minizip::dsl::from("README.md")
        | minizip::dsl::level(8)
        | minizip::dsl::save_to("dist");

    return plan.build().ok() ? 0 : 1;
}
