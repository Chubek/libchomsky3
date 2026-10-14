#include "Klyspec.hpp"
#include "SerdeTk-Frontend.hpp"

#include <iostream>

int main(int argc, char** argv) {
    klyspec::Registry registry;
    for (const char* name : {"serialize", "deserialize", "convert", "pack", "unpack", "compress",
                             "decompress", "validate", "formats", "schemas", "plugins"}) {
        registry.register_command({name, {}, {}, {}});
    }
    if (argc < 2) {
        std::cerr << "commands: serialize deserialize convert pack unpack compress decompress validate formats schemas plugins\n";
        return 2;
    }
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
    const auto parsed = klyspec::KlyCLIService(registry).parse(arguments.front(), {});
    if (!parsed.ok) return 2;
    return serdetk::frontend::execute(arguments, std::cout, std::cerr);
}
