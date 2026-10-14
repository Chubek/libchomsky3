#include "MiniZIP.hpp"

int main() {
    auto bytes = minizip::detail::read_file_bytes("dist/simple.mz");
    if (!bytes) {
        return 1;
    }

    auto opened = minizip::api::extractor::open(minizip::api::buffer_transport{*bytes});
    if (!opened.ok()) {
        return 1;
    }

    return opened.value().verify().ok() ? 0 : 1;
}
