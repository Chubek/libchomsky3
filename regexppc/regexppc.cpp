#include "regexppc.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        bool flex = false, simd = false;
        std::string input, output;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: regexppc [--re2c|--flex] [--simd] [-o OUTPUT] INPUT\n"
                    << "INPUT/OUTPUT may be - for standard input/output. SIMD emits C++ and requires a limit.\n";
                return 0;
            } else if (arg == "--version") { std::cout << "RegexPPC 1.0.0\n"; return 0; }
            else if (arg == "--flex" || arg == "-f") flex = true;
            else if (arg == "--re2c") flex = false;
            else if (arg == "--simd") simd = true;
            else if (arg == "-o" || arg == "--output") {
                if (++i == argc) throw std::runtime_error("missing output filename");
                output = argv[i];
            } else if (arg != "-" && !arg.empty() && arg[0] == '-') throw std::runtime_error("unknown option: " + arg);
            else if (!input.empty()) throw std::runtime_error("only one input file is accepted");
            else input = arg;
        }
        if (input.empty()) throw std::runtime_error("missing input file (use --help)");
        std::string source;
        if (input == "-") source.assign(std::istreambuf_iterator<char>(std::cin), {});
        else {
            std::ifstream file(input, std::ios::binary);
            if (!file) throw std::runtime_error("cannot read " + input);
            source.assign(std::istreambuf_iterator<char>(file), {});
        }
        std::string generated;
        if (flex) { auto cfg = regexppc::parse_flex(source); cfg.simd = simd; generated = regexppc::emit(cfg, "regexppc", true); }
        else generated = regexppc::transform_re2c(source, simd);
        if (output.empty() || output == "-") { std::cout << generated; if (!std::cout) throw std::runtime_error("output write failed"); }
        else {
            std::ofstream file(output, std::ios::binary);
            if (!file || !(file << generated)) throw std::runtime_error("cannot write " + output);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "regexppc: " << e.what() << '\n';
        return 1;
    }
}
