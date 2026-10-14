#ifndef QAMRPP_PODLET_PACKAGING_HPP
#define QAMRPP_PODLET_PACKAGING_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "../third_party/SerdeTk/include/MiniZIP.hpp"
#include "../third_party/SerdeTk/include/SerdeTk.hpp"

namespace qamrpp {
namespace podlet {

struct PackageOptions {
    std::string source_root;
    std::string output_qpod;
    bool include_all_files = true;
};

struct PackageResult {
    bool ok = false;
    std::string error;
    std::string output_qpod;
    std::size_t file_count = 0;
};

inline std::string trim_copy(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

inline std::string unquote_copy(std::string text) {
    text = trim_copy(text);
    if (text.size() >= 2) {
        const char first = text.front();
        const char last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return text.substr(1, text.size() - 2);
        }
    }
    return text;
}

inline bool read_file_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out, std::string& error) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.good()) {
        error = "failed to open file: " + path.string();
        return false;
    }
    out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

inline bool parse_qmr_manifest(const std::string& text, std::map<std::string, std::string>& out) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        const std::size_t colon = line.find(':');
        std::size_t sep = std::string::npos;
        if (eq != std::string::npos && colon != std::string::npos) {
            sep = std::min(eq, colon);
        } else if (eq != std::string::npos) {
            sep = eq;
        } else {
            sep = colon;
        }
        if (sep == std::string::npos) {
            continue;
        }
        const std::string key = trim_copy(line.substr(0, sep));
        const std::string value = unquote_copy(line.substr(sep + 1));
        if (!key.empty()) {
            out[key] = value;
        }
    }
    return true;
}

inline bool parse_lua_manifest(const std::string& text, std::map<std::string, std::string>& out) {
    static const std::regex assign_pattern("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+?)\\s*,?\\s*$");
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || (line.size() >= 2 && line[0] == '-' && line[1] == '-')) {
            continue;
        }
        std::smatch match;
        if (!std::regex_match(line, match, assign_pattern)) {
            continue;
        }
        const std::string key = match[1].str();
        std::string value = match[2].str();
        if (value == "{" || value == "}") {
            continue;
        }
        out[key] = unquote_copy(value);
    }
    return true;
}

inline bool read_manifest_file(
    const std::filesystem::path& root,
    std::filesystem::path& manifest_path_out,
    std::map<std::string, std::string>& manifest_out,
    std::string& error
) {
    const std::filesystem::path qmr = root / "Podpack.qmr";
    const std::filesystem::path lua = root / "Podpack.lua";
    std::filesystem::path manifest_path;
    if (std::filesystem::exists(qmr)) {
        manifest_path = qmr;
    } else if (std::filesystem::exists(lua)) {
        manifest_path = lua;
    } else {
        error = "missing required manifest: Podpack.qmr or Podpack.lua";
        return false;
    }

    std::ifstream in(manifest_path, std::ios::in | std::ios::binary);
    if (!in.good()) {
        error = "failed to open manifest: " + manifest_path.string();
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    manifest_out.clear();
    if (manifest_path.extension() == ".qmr") {
        (void)parse_qmr_manifest(text, manifest_out);
    } else {
        (void)parse_lua_manifest(text, manifest_out);
    }
    manifest_path_out = manifest_path;
    return true;
}

inline bool validate_manifest(std::map<std::string, std::string>& manifest, std::string& error) {
    if (manifest.find("name") == manifest.end() || manifest["name"].empty()) {
        error = "manifest field `name` is required";
        return false;
    }
    if (manifest.find("version") == manifest.end() || manifest["version"].empty()) {
        error = "manifest field `version` is required";
        return false;
    }
    if (manifest.find("entrypoint") == manifest.end() || manifest["entrypoint"].empty()) {
        error = "manifest field `entrypoint` is required";
        return false;
    }
    if (manifest.find("podlet_api") == manifest.end()) {
        manifest["podlet_api"] = "1";
    }
    if (manifest.find("format_version") == manifest.end()) {
        manifest["format_version"] = "1";
    }
    return true;
}

inline bool collect_package_files(
    const std::filesystem::path& root,
    const std::filesystem::path& output_qpod,
    bool include_all_files,
    std::vector<std::string>& relative_paths_out,
    std::string& error
) {
    const std::filesystem::path required_entry = root / "Podlet.cpp";
    if (!std::filesystem::exists(required_entry)) {
        error = "missing required source file: Podlet.cpp";
        return false;
    }

    std::vector<std::string> rels;
    if (include_all_files) {
        for (std::filesystem::recursive_directory_iterator it(root), end; it != end; ++it) {
            if (!it->is_regular_file()) {
                continue;
            }
            const std::filesystem::path abs = it->path();
            if (abs == output_qpod) {
                continue;
            }
            if (abs.extension() == ".qpod") {
                continue;
            }
            std::filesystem::path rel = std::filesystem::relative(abs, root);
            rels.push_back(rel.generic_string());
        }
    } else {
        rels.push_back("Podlet.cpp");
    }
    std::sort(rels.begin(), rels.end());
    rels.erase(std::unique(rels.begin(), rels.end()), rels.end());
    if (std::find(rels.begin(), rels.end(), "Podlet.cpp") == rels.end()) {
        rels.insert(rels.begin(), "Podlet.cpp");
    }
    relative_paths_out = rels;
    return true;
}

inline PackageResult build_qpod(const PackageOptions& options) {
    PackageResult result;
    result.output_qpod = options.output_qpod;

    if (options.source_root.empty()) {
        result.error = "source_root is required";
        return result;
    }
    if (options.output_qpod.empty()) {
        result.error = "output_qpod is required";
        return result;
    }

    const std::filesystem::path root = std::filesystem::path(options.source_root);
    const std::filesystem::path out = std::filesystem::path(options.output_qpod);
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        result.error = "source_root is not a directory: " + root.string();
        return result;
    }

    std::filesystem::path manifest_path;
    std::map<std::string, std::string> manifest;
    if (!read_manifest_file(root, manifest_path, manifest, result.error)) {
        return result;
    }
    if (!validate_manifest(manifest, result.error)) {
        return result;
    }

    const std::filesystem::path entrypoint_abs = root / manifest["entrypoint"];
    if (!std::filesystem::exists(entrypoint_abs)) {
        result.error = "manifest entrypoint does not exist: " + manifest["entrypoint"];
        return result;
    }

    std::vector<std::string> file_paths;
    if (!collect_package_files(root, out, options.include_all_files, file_paths, result.error)) {
        return result;
    }

    minizip::api::zipper zipper = minizip::api::zipper::make_zipper();
    zipper.set_codec(minizip::backend::codec::zstd);
    zipper.set_archive_name(out.filename().string());
    zipper.set_destination(out.parent_path());

    std::string manifest_text;
    {
        std::ostringstream manifest_stream;
        for (std::map<std::string, std::string>::const_iterator it = manifest.begin(); it != manifest.end(); ++it) {
            manifest_stream << it->first << " = " << it->second << "\n";
        }
        manifest_text = manifest_stream.str();
    }
    zipper.add_bytes(
        "meta/Podpack.qmr",
        std::as_bytes(std::span<const char>(manifest_text.data(), manifest_text.size())),
        "text/plain"
    );

    for (std::size_t i = 0; i < file_paths.size(); ++i) {
        std::vector<std::uint8_t> bytes;
        const std::filesystem::path abs = root / file_paths[i];
        if (!read_file_bytes(abs, bytes, result.error)) {
            return result;
        }
        zipper.add_bytes(
            file_paths[i],
            std::as_bytes(std::span<const std::uint8_t>(bytes.data(), bytes.size())),
            "application/octet-stream"
        );
    }

    const std::filesystem::path parent = out.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            result.error = "failed to create output directory: " + parent.string();
            return result;
        }
    }

    try {
        const minizip::api::archive_result archive = zipper.build();
        if (!archive.ok()) {
            result.error = archive.message;
            return result;
        }
    } catch (const std::exception& e) {
        result.error = std::string("failed to build qpod: ") + e.what();
        return result;
    }

    result.ok = true;
    result.file_count = file_paths.size() + 1;
    return result;
}

} // namespace podlet
} // namespace qamrpp

#endif // QAMRPP_PODLET_PACKAGING_HPP
