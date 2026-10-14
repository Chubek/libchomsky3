#include <glob.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../QaMRpp.hpp"
#include "../plugins/QaMRpp-Dump.hpp"
#include "../plugins/QaMRpp-Readline.hpp"
#include "../plugins/QaMRpp-Serialize2JSON.hpp"
#include "../plugins/QaMRpp-QBF.hpp"

static std::vector<std::string> expand_glob(const std::string& pat) {
    glob_t g;
    std::vector<std::string> out;
    if (glob(pat.c_str(), 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) out.push_back(g.gl_pathv[i]);
    }
    globfree(&g);
    return out;
}

int main(int argc, char** argv) {
    qamrpp::Context ctx;
    qamrpp::DumpPlugin dump_plugin;
    qamrpp::Serialize2JSONPlugin serialize_plugin;
    dump_plugin.install(ctx);
    serialize_plugin.install(ctx);

    std::vector<std::string> scripts;
    std::vector<std::string> required_files;
    std::vector<std::string> libraries;
    std::string dump_file;
    bool serialize = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--script" || arg == "-s") {
            if (i + 1 >= argc) throw std::runtime_error("--script expects a glob pattern");
            std::vector<std::string> m = expand_glob(argv[++i]);
            scripts.insert(scripts.end(), m.begin(), m.end());
        } else if (arg == "--load") {
            if (i + 1 >= argc) throw std::runtime_error("--load expects library name/path");
            libraries.push_back(argv[++i]);
        } else if (arg == "--require") {
            if (i + 1 >= argc) throw std::runtime_error("--require expects script path");
            required_files.push_back(argv[++i]);
        } else if (arg == "--dump") {
            if (i + 1 >= argc) throw std::runtime_error("--dump expects output file");
            dump_file = argv[++i];
            std::vector<qamrpp::ValuePtr> args;
            args.push_back(std::make_shared<qamrpp::Value>(dump_file));
            ctx.globals["set_dump_file"]->function_value(ctx, args);
        } else if (arg == "--serialize") {
            serialize = true;
        } else if (arg == "--qbf") {
            if (i + 1 >= argc) throw std::runtime_error("--qbf expects bundle path");
            std::vector<qamrpp::QBFEntry> entries;
            if (!qamrpp::read_qbf(argv[++i], entries)) throw std::runtime_error("failed to read qbf");
            for (size_t k = 0; k < entries.size(); ++k) ctx.linker.add_source(entries[k].path, entries[k].source);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    for (size_t i = 0; i < libraries.size(); ++i) {
        if (!ctx.load_library(libraries[i]) && !ctx.load_library_named(libraries[i])) {
            throw std::runtime_error("failed to load library: " + libraries[i]);
        }
    }
    for (size_t i = 0; i < required_files.size(); ++i) {
        if (!ctx.linker.add_file(required_files[i])) {
            throw std::runtime_error("cannot open required file: " + required_files[i]);
        }
    }

    if (scripts.empty() && ctx.linker.size() == 0) {
        qamrpp::Readline rl;
        while (true) {
            std::string line = rl.readline("qamrpp> ");
            if (line == "exit" || line == "quit") break;
            if (line.empty()) continue;
            (void)ctx.run(line);
        }
        return 0;
    }

    for (size_t i = 0; i < scripts.size(); ++i) {
        if (!ctx.linker.add_file(scripts[i])) throw std::runtime_error("cannot open script: " + scripts[i]);
    }
    (void)ctx.linker.link(ctx);

    if (serialize) {
        std::vector<qamrpp::ValuePtr> noargs;
        qamrpp::ValuePtr v = ctx.globals["get_serialized_program"]->function_value(ctx, noargs);
        std::cout << v->to_string() << "\n";
    }
    return 0;
}
