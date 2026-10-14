#include "MiniZIP.hpp"

int main() {
    auto zipper = minizip::api::zipper::make_zipper(
        minizip::api::speed::Balanced,
        minizip::api::focus::Compression
    );
    zipper.set_codec(minizip::backend::codec::zstd);
    zipper.add_file("README.md");

    auto result = zipper.build_file("artifacts/explicit-path.mz");
    return result.ok() ? 0 : 1;
}
