#include "MiniZIP.hpp"

int main() {
    auto opened = minizip::api::extractor::open("dist/bytes-only.mz");
    if (!opened.ok()) {
        return 1;
    }

    auto payload = opened.value().extract_bytes("generated/config.json");
    return payload.ok() ? 0 : 1;
}
