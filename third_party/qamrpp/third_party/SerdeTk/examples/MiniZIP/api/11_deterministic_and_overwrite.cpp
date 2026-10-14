#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper(
        minizip::api::speed::Best,
        minizip::api::focus::Archiving
    );
    zipper.deterministic(true);
    zipper.overwrite_policy(minizip::api::overwrite::Replace);
    zipper.set_codec(minizip::backend::codec::zstd);
    zipper.add_file("README.md");
    zipper.set_destination("dist");
    zipper.set_archive_name("deterministic.mz");

    return zipper.build().ok() ? 0 : 1;
}
