#include "MiniZIP.hpp"

#include <cstdint>

int main() {
    minizip::api::filter xor_filter;
    xor_filter.tag = "xor-demo";
    xor_filter.encode = [](std::span<const std::byte> bytes) -> minizip::api::result<minizip::detail::byte_vector> {
        minizip::detail::byte_vector out(bytes.begin(), bytes.end());
        for (auto &byte : out) {
            byte ^= std::byte{0x5A};
        }
        return out;
    };
    xor_filter.decode = xor_filter.encode;

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.with_filter(std::move(xor_filter));
    zipper.add_bytes("secret/payload.txt", minizip::detail::to_bytes("hello filter"));
    zipper.set_destination("dist");
    zipper.set_archive_name("filtered.mz");

    return zipper.build().ok() ? 0 : 1;
}
