#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_file("README.md");
    zipper.set_destination("dist");
    zipper.set_archive_name("simple.mz");

    auto result = zipper.build();
    return result.ok() ? 0 : 1;
}
