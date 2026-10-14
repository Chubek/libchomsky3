#include "MiniZIP.hpp"

#include <string>
#include <vector>

int main() {
    std::vector<std::string> events;

    minizip::api::filter passthrough;
    passthrough.tag = "passthrough";
    passthrough.encode = [](std::span<const std::byte> bytes) -> minizip::api::result<minizip::detail::byte_vector> {
        return minizip::detail::byte_vector(bytes.begin(), bytes.end());
    };
    passthrough.decode = passthrough.encode;

    minizip::api::progress_hook hook;
    hook.on_entry = [&events](std::string_view path) { events.emplace_back(path); };

    auto plan =
        minizip::dsl::archive("dsl-filtered.mz")
        | minizip::dsl::from("README.md")
        | minizip::dsl::filter_with(std::move(passthrough))
        | minizip::dsl::hook_with(std::move(hook))
        | minizip::dsl::save_to("dist");

    auto result = plan.build();
    return result.ok() && !events.empty() ? 0 : 1;
}
