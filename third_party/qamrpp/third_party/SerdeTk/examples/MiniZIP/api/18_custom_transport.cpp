#include "MiniZIP.hpp"

#include <map>
#include <memory>
#include <string>

int main() {
    auto sink = std::make_shared<std::map<std::string, minizip::detail::byte_vector>>();

    minizip::api::transport transport;
    transport.tag = "memory-map";
    transport.write = [sink](std::string_view path, std::span<const std::byte> bytes) -> minizip::api::result<void> {
        (*sink)[std::string(path)] = minizip::detail::byte_vector(bytes.begin(), bytes.end());
        return {};
    };

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_file("README.md");
    zipper.with_transport(std::move(transport));
    zipper.set_destination("virtual");
    zipper.set_archive_name("transported.mz");

    auto result = zipper.build();
    return result.ok() && sink->contains("virtual/transported.mz") ? 0 : 1;
}
