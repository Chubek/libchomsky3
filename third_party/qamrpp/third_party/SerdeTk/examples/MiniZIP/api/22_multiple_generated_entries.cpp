#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_bytes("generated/a.txt", minizip::detail::to_bytes("alpha"));
    zipper.add_bytes("generated/b.txt", minizip::detail::to_bytes("beta"));
    zipper.add_bytes("generated/c.txt", minizip::detail::to_bytes("gamma"));
    zipper.create_manifest<minizip::api::INIManifest>();
    zipper.add_to_manifest(".section generated");
    zipper.add_to_manifest(".kv count 3");
    zipper.set_destination("dist");
    zipper.set_archive_name("generated-batch.mz");

    return zipper.build().ok() ? 0 : 1;
}
