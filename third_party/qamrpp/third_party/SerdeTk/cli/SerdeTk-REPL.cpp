#include "SerdeTk-Frontend.hpp"
#include "PikoRL.hpp"

#include <iostream>
#include <sstream>

int main() {
    std::string line;
    while (std::cout << "serdetk> " && std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") return 0;
        std::istringstream input(line);
        std::vector<std::string> arguments;
        for (std::string token; input >> token; ) arguments.push_back(std::move(token));
        if (!arguments.empty()) serdetk::frontend::execute(arguments, std::cout, std::cerr);
    }
    return 0;
}
