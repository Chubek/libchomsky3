#include "MiniZIP.hpp"

#include <any>
#include <string>

struct package_info {
    std::string name;
    int version = 0;
};

int main() {
    minizip::api::deserializer decoder;
    decoder.tag = "package-info";
    decoder.decode_any = [](std::span<const std::byte> bytes, std::any &target) -> minizip::api::result<void> {
        auto ref = std::any_cast<std::reference_wrapper<package_info>>(target);
        auto text = minizip::detail::to_string_lossy(bytes);
        auto split = text.find(':');
        if (split == std::string::npos) {
            return minizip::api::result<void>("invalid package-info payload");
        }
        ref.get().name = text.substr(0, split);
        ref.get().version = std::stoi(text.substr(split + 1));
        return {};
    };

    auto opened = minizip::api::extractor::open("dist/object-archive.mz");
    if (!opened.ok()) {
        return 1;
    }

    package_info info;
    auto extractor = std::move(opened.value());
    extractor.with_deserializer(std::move(decoder));
    auto result = extractor.extract("meta/package.dat", info);

    return result.ok() ? 0 : 1;
}
