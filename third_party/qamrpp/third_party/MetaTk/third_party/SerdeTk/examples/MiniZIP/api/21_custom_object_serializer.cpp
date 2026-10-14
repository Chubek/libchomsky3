#include "MiniZIP.hpp"

#include <any>
#include <string>

struct package_info {
    std::string name;
    int version = 0;
};

int main() {
    minizip::api::serializer encoder;
    encoder.tag = "package-info";
    encoder.encode_any = [](const std::any &value) -> minizip::api::result<minizip::detail::byte_vector> {
        const auto &info = std::any_cast<const package_info &>(value);
        return minizip::detail::to_bytes(info.name + ":" + std::to_string(info.version));
    };

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.add_object("meta/package.dat", package_info{"MiniZIP", 1}, std::move(encoder));
    zipper.set_destination("dist");
    zipper.set_archive_name("object-archive.mz");

    return zipper.build().ok() ? 0 : 1;
}
