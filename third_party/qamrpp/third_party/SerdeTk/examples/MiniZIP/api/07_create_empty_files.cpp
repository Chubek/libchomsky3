#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.create_text_file("docs/placeholder.md");
    zipper.create_binary_file("cache/empty.bin");
    zipper.set_destination("dist");
    zipper.set_archive_name("empty-files.mz");

    return zipper.build().ok() ? 0 : 1;
}
