#include "MiniZIP.hpp"

#include <span>
#include <string>

int main() {
    std::string text = "MiniZIP backend round-trip example";

    auto compressed = minizip::backend::zstd_engine::compress(
        std::as_bytes(std::span{text.data(), text.size()}),
        {.algorithm = minizip::backend::codec::zstd}
    );
    if (!compressed.ok()) {
        return 1;
    }

    auto decompressed = minizip::backend::zstd_engine::decompress(
        compressed.bytes,
        minizip::backend::codec::zstd
    );
    if (!decompressed.ok()) {
        return 1;
    }

    return minizip::detail::to_string_lossy(decompressed.bytes) == text ? 0 : 1;
}
