#include "MiniZIP.hpp"

#include <span>
#include <string>

int main() {
    std::string text = "inspectable raw-block payload";

    auto compressed = minizip::backend::zstd_engine::compress(
        std::as_bytes(std::span{text.data(), text.size()}),
        {.algorithm = minizip::backend::codec::zstd}
    );
    if (!compressed.ok()) {
        return 1;
    }

    auto info = minizip::backend::zstd_engine::inspect_frame(compressed.bytes);
    if (!info.has_value()) {
        return 1;
    }

    return info->valid && !info->blocks.empty() ? 0 : 1;
}
