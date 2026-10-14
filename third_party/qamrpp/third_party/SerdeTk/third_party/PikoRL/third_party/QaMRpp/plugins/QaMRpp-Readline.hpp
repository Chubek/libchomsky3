#ifndef QAMRPP_READLINE_HPP
#define QAMRPP_READLINE_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace qamrpp {

class Readline {
public:
    std::string readline(const std::string& prompt) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return std::string();
        }
        return line;
    }

    bool is_interactive() const {
        return true;
    }

    void load_history(const std::string& path) {
        history_.clear();
        std::ifstream in(path.c_str());
        std::string line;
        while (std::getline(in, line)) history_.push_back(line);
    }

    void save_history(const std::string& path) {
        std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        for (size_t i = 0; i < history_.size(); ++i) out << history_[i] << "\n";
    }

    void add_history(const std::string& line) {
        if (!line.empty()) history_.push_back(line);
    }

private:
    std::vector<std::string> history_;
};

} // namespace qamrpp

#endif
