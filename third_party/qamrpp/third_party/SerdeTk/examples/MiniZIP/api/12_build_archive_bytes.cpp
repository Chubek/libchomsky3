#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_file("README.md");

    auto archive = zipper.build_archive_bytes();
    if (!archive.ok()) {
        return 1;
    }

    return archive.value().empty() ? 1 : 0;
}
