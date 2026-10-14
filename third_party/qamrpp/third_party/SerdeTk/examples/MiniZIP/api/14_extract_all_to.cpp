#include "MiniZIP.hpp"

int main() {
    auto opened = minizip::api::extractor::open("dist/simple.mz");
    if (!opened.ok()) {
        return 1;
    }

    auto result = opened.value().extract_all_to("out/simple");
    return result.ok() ? 0 : 1;
}
