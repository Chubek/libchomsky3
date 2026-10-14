#include "MiniZIP.hpp"

int main() {
    auto opened = minizip::api::extractor::open("dist/simple.mz");
    if (!opened.ok()) {
        return 1;
    }

    auto extractor = std::move(opened.value());
    auto listed = extractor.list_entries();
    auto verified = extractor.verify();

    return (!listed.empty() && verified.ok()) ? 0 : 1;
}
