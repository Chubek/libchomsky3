#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_file("README.md");
    zipper.create_manifest<minizip::api::INIManifest>();
    zipper.add_to_manifest(".section package");
    zipper.add_to_manifest(".kv name MiniZIP");
    zipper.add_to_manifest(".kv version 1.0.0");
    zipper.add_to_manifest(".section build");
    zipper.add_to_manifest(".kv channel release");
    zipper.set_destination("dist");
    zipper.set_archive_name("with-manifest.mz");

    return zipper.build().ok() ? 0 : 1;
}
