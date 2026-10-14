#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace jiterati::third_party::zstd {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline std::string shell_quote(std::filesystem::path const& path) {
    std::string text = path.string();
    std::string quoted = "'";
    for (char ch : text) {
        if (ch == '\'') quoted += "'\\''";
        else quoted += ch;
    }
    quoted += "'";
    return quoted;
}

inline bool available() {
    return std::system("command -v zstd >/dev/null 2>&1") == 0;
}

inline void require_available() {
    if (!available()) throw Error("zstd executable is required for .jpkg packages");
}

inline void compress(std::filesystem::path const& input, std::filesystem::path const& output) {
    require_available();
    std::string command = "zstd -q -f -o " + shell_quote(output) + " " + shell_quote(input);
    if (std::system(command.c_str()) != 0) throw Error("zstd failed to compress package: " + output.string());
}

inline void decompress(std::filesystem::path const& input, std::filesystem::path const& output) {
    require_available();
    std::string command = "zstd -q -d -f -o " + shell_quote(output) + " " + shell_quote(input);
    if (std::system(command.c_str()) != 0) throw Error("zstd failed to decompress package: " + input.string());
}

} // namespace jiterati::third_party::zstd
