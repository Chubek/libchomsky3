#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_directory("assets", minizip::api::options::Recursive);
    zipper.set_destination("dist");
    zipper.set_archive_name("assets-recursive.mz");

    return zipper.build().ok() ? 0 : 1;
}
