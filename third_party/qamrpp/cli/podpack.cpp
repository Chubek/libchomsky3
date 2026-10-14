#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../podlet/PodletPackaging.hpp"

namespace {

void print_help() {
    std::cout
        << "Usage:\n"
        << "  podpack [source_dir] [--output <file.qpod>]\n"
        << "  podpack --init [dir]\n"
        << "  podpack --install <file.qpod> [--root <install_root>]\n\n"
        << "Notes:\n"
        << "  - build: packages a Podlet directory into a .qpod archive\n"
        << "  - init:  scaffolds Podlet.cpp and Podpack.qmr\n"
        << "  - install: copies archive into QAMRPP_HOME/podlet (or ~/.qamrpp/podlet if unset) (default)\n";
}

std::string default_install_root() {
    const char* home_env = std::getenv("QAMRPP_HOME");
    if (home_env && *home_env) {
        return std::string(home_env) + "/podlet";
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.qamrpp/podlet";
    }
    return ".qamrpp/podlet";
}

bool write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        return false;
    }
    out << text;
    return out.good();
}

std::string infer_name_from_dir(const std::filesystem::path& dir) {
    const std::string stem = dir.filename().string();
    if (stem.empty()) {
        return "my_podlet";
    }
    std::string out;
    for (std::size_t i = 0; i < stem.size(); ++i) {
        const char c = stem[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "my_podlet" : out;
}

std::string replace_or_add_qpod_extension(const std::filesystem::path& input) {
    std::filesystem::path out = input;
    out.replace_extension(".qpod");
    return out.string();
}

int run_init(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "podpack: failed to create directory: " << dir.string() << "\n";
        return 2;
    }

    const std::filesystem::path podlet_cpp = dir / "Podlet.cpp";
    const std::filesystem::path manifest = dir / "Podpack.qmr";
    if (std::filesystem::exists(podlet_cpp) || std::filesystem::exists(manifest)) {
        std::cerr << "podpack: target already contains Podlet.cpp or Podpack.qmr\n";
        return 3;
    }

    const std::string name = infer_name_from_dir(dir);
    const std::string cpp_text =
        "#include \"QaMRpp-Library.hpp\"\n\n"
        "// Podlet source entrypoint payload (Stage 3 packaging).\n"
        "// Future stages can compile/link this into richer runtime exports.\n";
    const std::string manifest_text =
        "name = " + name + "\n"
        "version = 0.1.0\n"
        "entrypoint = Podlet.cpp\n"
        "podlet_api = 1\n"
        "format_version = 1\n";

    if (!write_text_file(podlet_cpp, cpp_text) || !write_text_file(manifest, manifest_text)) {
        std::cerr << "podpack: failed to write scaffold files\n";
        return 4;
    }

    std::cout << "initialized podlet scaffold in " << dir.string() << "\n";
    return 0;
}

int run_install(const std::filesystem::path& qpod, const std::filesystem::path& root) {
    if (!std::filesystem::exists(qpod) || !std::filesystem::is_regular_file(qpod)) {
        std::cerr << "podpack: qpod file does not exist: " << qpod.string() << "\n";
        return 2;
    }
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        std::cerr << "podpack: failed to create install root: " << root.string() << "\n";
        return 3;
    }

    const std::filesystem::path target = root / qpod.filename();
    std::filesystem::copy_file(qpod, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "podpack: failed to install qpod: " << target.string() << "\n";
        return 4;
    }
    std::cout << "installed " << qpod.string() << " -> " << target.string() << "\n";
    return 0;
}

int run_build(const std::filesystem::path& source_dir, const std::string& output_override) {
    const std::filesystem::path out =
        output_override.empty()
            ? std::filesystem::path(replace_or_add_qpod_extension(source_dir / infer_name_from_dir(source_dir)))
            : std::filesystem::path(output_override);

    qamrpp::podlet::PackageOptions options;
    options.source_root = source_dir.string();
    options.output_qpod = out.string();
    options.include_all_files = true;
    const qamrpp::podlet::PackageResult result = qamrpp::podlet::build_qpod(options);
    if (!result.ok) {
        std::cerr << "podpack: " << result.error << "\n";
        return 2;
    }
    std::cout << "built " << result.output_qpod << " (" << result.file_count << " files)\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            return run_build(".", "");
        }

        bool mode_init = false;
        bool mode_install = false;
        std::string source_dir = ".";
        std::string output_qpod;
        std::string install_qpod;
        std::string install_root = default_install_root();

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                print_help();
                return 0;
            } else if (arg == "--init") {
                mode_init = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    source_dir = argv[++i];
                }
            } else if (arg == "--install") {
                mode_install = true;
                if (i + 1 >= argc) {
                    std::cerr << "podpack: --install expects .qpod path\n";
                    return 1;
                }
                install_qpod = argv[++i];
            } else if (arg == "--root") {
                if (i + 1 >= argc) {
                    std::cerr << "podpack: --root expects directory\n";
                    return 1;
                }
                install_root = argv[++i];
            } else if (arg == "--output") {
                if (i + 1 >= argc) {
                    std::cerr << "podpack: --output expects .qpod path\n";
                    return 1;
                }
                output_qpod = argv[++i];
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "podpack: unknown option: " << arg << "\n";
                return 1;
            } else {
                source_dir = arg;
            }
        }

        if (mode_init && mode_install) {
            std::cerr << "podpack: --init and --install are mutually exclusive\n";
            return 1;
        }

        if (mode_init) {
            return run_init(source_dir);
        }
        if (mode_install) {
            return run_install(install_qpod, install_root);
        }
        return run_build(source_dir, output_qpod);
    } catch (const std::exception& e) {
        std::cerr << "podpack: " << e.what() << "\n";
        return 1;
    }
}
