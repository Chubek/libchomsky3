#include "MiniZIP.hpp"

int main() {
    auto config = minizip::detail::to_bytes(R"({"name":"MiniZIP","ok":true})");

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_bytes("generated/config.json", config, "application/json");
    zipper.set_destination("dist");
    zipper.set_archive_name("bytes-only.mz");

    return zipper.build().ok() ? 0 : 1;
}
