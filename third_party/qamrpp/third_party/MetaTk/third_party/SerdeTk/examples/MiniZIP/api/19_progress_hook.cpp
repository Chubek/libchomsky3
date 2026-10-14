#include "MiniZIP.hpp"

#include <string>
#include <vector>

int main() {
    std::vector<std::string> visited;

    minizip::api::progress_hook hook;
    hook.on_entry = [&visited](std::string_view path) {
        visited.emplace_back(path);
    };
    hook.on_finish = [&visited]() {
        visited.emplace_back("done");
    };

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.with_hook(std::move(hook));
    zipper.add_file("README.md");
    zipper.set_destination("dist");
    zipper.set_archive_name("hooked.mz");

    auto result = zipper.build();
    return result.ok() && !visited.empty() ? 0 : 1;
}
