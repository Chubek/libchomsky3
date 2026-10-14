#include "MiniZIP.hpp"

int main() {
    auto plan =
        minizip::dsl::archive("dsl-manifest.mz")
        | minizip::dsl::from("README.md")
        | minizip::dsl::manifest<minizip::api::INIManifest>()
        | minizip::dsl::manifest_cmd(".section package")
        | minizip::dsl::manifest_cmd(".kv name MiniZIP")
        | minizip::dsl::manifest_cmd(".kv style dsl")
        | minizip::dsl::save_to("dist");

    return plan.build().ok() ? 0 : 1;
}
