#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_file("README.md");
    zipper.seal();
    zipper.reopen();
    zipper.create_text_file("notes/after-reopen.txt");
    zipper.set_destination("dist");
    zipper.set_archive_name("reopened.mz");

    return zipper.build().ok() ? 0 : 1;
}
