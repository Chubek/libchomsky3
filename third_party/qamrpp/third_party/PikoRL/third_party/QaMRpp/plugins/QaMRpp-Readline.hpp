#ifndef QAMRPP_READLINE_HPP
#define QAMRPP_READLINE_HPP

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

#ifndef _WIN32
#include <cstdio>
#include <termios.h>
#include <unistd.h>
#endif

namespace qamrpp {

class Readline {
public:
    using Completer = std::function<std::vector<std::string>(const std::string&)>;

    struct Syntax {
        std::set<std::string> keywords;
        std::set<std::string> operators;
        std::set<std::string> literals;
    };

    Readline() {
        syntax_.keywords = {
            "if", "else", "elseif", "while", "for", "function", "return",
            "local", "then", "do", "end", "in", "and", "or", "not"
        };
        syntax_.literals = {"true", "false", "nil"};
    }

    static bool terminal_interactive() {
#ifndef _WIN32
        return ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO);
#else
        return false;
#endif
    }

    static std::string color(const std::string& text, const char* code) {
        return std::string("\033[") + code + "m" + text + "\033[0m";
    }

    std::string highlight(const std::string& line) const {
        if (!use_color_) return line;
        std::string out;
        for (size_t i = 0; i < line.size();) {
            unsigned char ch = static_cast<unsigned char>(line[i]);
            if (line[i] == '-' && i + 1 < line.size() && line[i + 1] == '-') {
                out += color(line.substr(i), "90");
                break;
            }
            if (line[i] == '"') {
                size_t j = i + 1;
                while (j < line.size() && line[j] != '"') ++j;
                if (j < line.size()) ++j;
                out += color(line.substr(i, j - i), "32");
                i = j;
                continue;
            }
            if (std::isdigit(ch)) {
                size_t j = i + 1;
                while (j < line.size() && (std::isdigit(static_cast<unsigned char>(line[j])) || line[j] == '.')) ++j;
                out += color(line.substr(i, j - i), "36");
                i = j;
                continue;
            }
            if (std::isalpha(ch) || line[i] == '_') {
                size_t j = i + 1;
                while (j < line.size() && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_')) ++j;
                std::string word = line.substr(i, j - i);
                if (syntax_.keywords.count(word)) {
                    out += color(word, "1;35");
                } else if (syntax_.literals.count(word)) {
                    out += color(word, "1;33");
                } else {
                    out += word;
                }
                i = j;
                continue;
            }
            if (std::ispunct(ch)) {
                out += color(line.substr(i, 1), "34");
            } else {
                out += line[i];
            }
            ++i;
        }
        return out;
    }

    bool read_line(const std::string& prompt, std::string& out, const std::string& initial = std::string()) {
        if (!terminal_interactive()) {
            std::cout << prompt;
            std::cout.flush();
            return static_cast<bool>(std::getline(std::cin, out));
        }
#ifndef _WIN32
        struct termios old_term;
        if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
            std::cout << prompt;
            std::cout.flush();
            return static_cast<bool>(std::getline(std::cin, out));
        }
        struct termios raw = old_term;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
            std::cout << prompt;
            std::cout.flush();
            return static_cast<bool>(std::getline(std::cin, out));
        }

        std::string line = initial;
        size_t cursor = line.size();
        size_t history_index = history_.size();
        auto redraw = [&]() {
            std::cout << "\r\033[2K" << prompt << highlight(line);
            const size_t visual_end = prompt.size() + line.size();
            const size_t visual_cursor = prompt.size() + cursor;
            if (visual_end > visual_cursor) {
                std::cout << "\033[" << (visual_end - visual_cursor) << "D";
            }
            std::cout.flush();
        };

        std::cout << prompt << highlight(line) << std::flush;
        bool ok = true;
        while (true) {
            char c = 0;
            ssize_t got = ::read(STDIN_FILENO, &c, 1);
            if (got <= 0) {
                ok = false;
                break;
            }
            if (c == '\n' || c == '\r') {
                std::cout << "\r\033[2K" << prompt << highlight(line) << "\n";
                break;
            }
            if (c == 4) {
                if (line.empty()) {
                    ok = false;
                    break;
                }
                continue;
            }
            if (c == 3) {
                line.clear();
                std::cout << "^C\n";
                break;
            }
            if (c == 127 || c == 8) {
                if (cursor > 0) {
                    line.erase(cursor - 1, 1);
                    --cursor;
                    redraw();
                }
                continue;
            }
            if (c == '\t') {
                std::string prefix = current_prefix(line, cursor);
                std::vector<std::string> matches = complete(prefix);
                if (matches.size() == 1 && matches[0].size() > prefix.size()) {
                    line.insert(cursor, matches[0].substr(prefix.size()));
                    cursor += matches[0].size() - prefix.size();
                    redraw();
                } else if (!matches.empty()) {
                    std::cout << "\n";
                    for (size_t i = 0; i < matches.size(); ++i) std::cout << matches[i] << (i + 1 == matches.size() ? "\n" : "  ");
                    redraw();
                }
                continue;
            }
            if (c == 27) {
                char seq[2] = {0, 0};
                if (::read(STDIN_FILENO, &seq[0], 1) <= 0 || ::read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                if (seq[0] == '[') {
                    if (seq[1] == 'D' && cursor > 0) --cursor;
                    if (seq[1] == 'C' && cursor < line.size()) ++cursor;
                    if (seq[1] == 'A' && !history_.empty() && history_index > 0) {
                        --history_index;
                        line = history_[history_index];
                        cursor = line.size();
                    }
                    if (seq[1] == 'B' && history_index < history_.size()) {
                        ++history_index;
                        line = history_index < history_.size() ? history_[history_index] : std::string();
                        cursor = line.size();
                    }
                    redraw();
                }
                continue;
            }
            if (std::isprint(static_cast<unsigned char>(c))) {
                line.insert(cursor, 1, c);
                ++cursor;
                redraw();
            }
        }

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
        out = line;
        return ok;
#else
        std::cout << prompt;
        std::cout.flush();
        return static_cast<bool>(std::getline(std::cin, out));
#endif
    }

    static std::string polyrl_read(const std::string& prompt) {
        Readline reader;
        std::string line;
        (void)reader.read_line(prompt, line);
        return line;
    }

    static void polyrl_history_add(std::vector<std::string>& history, const std::string& line) {
        if (!line.empty()) {
            history.push_back(line);
        }
    }

    static std::vector<std::string> polyrl_complete(const std::string& prefix, const Completer& cb) {
        return cb ? cb(prefix) : std::vector<std::string>{};
    }

    void set_completer(Completer cb) {
        completer_ = std::move(cb);
    }

    std::vector<std::string> complete(const std::string& prefix) const {
        return polyrl_complete(prefix, completer_);
    }

    std::string readline(const std::string& prompt) {
        std::string line;
        (void)read_line(prompt, line);
        return line;
    }

    std::string readline(const std::string& prompt, const std::string& initial) {
        std::string line;
        (void)read_line(prompt, line, initial);
        return line;
    }

    bool is_interactive() const {
        return terminal_interactive();
    }

    void set_color(bool value) {
        use_color_ = value;
    }

    bool load_syntax(const std::string& path) {
        std::ifstream in(path.c_str());
        if (!in) return false;
        std::string section;
        std::string line;
        while (std::getline(in, line)) {
            trim_in_place(line);
            if (line.empty() || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }
            if (section == "keywords" || section == "literals" || section == "operators") {
                std::istringstream ss(line);
                std::string word;
                while (ss >> word) {
                    if (section == "keywords") syntax_.keywords.insert(word);
                    else if (section == "literals") syntax_.literals.insert(word);
                    else syntax_.operators.insert(word);
                }
            }
        }
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
        polyrl_history_add(history_, line);
    }

private:
    std::vector<std::string> history_;
    Completer completer_;
    Syntax syntax_;
    bool use_color_ = true;

    static std::string current_prefix(const std::string& line, size_t cursor) {
        size_t start = cursor;
        while (start > 0) {
            char c = line[start - 1];
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') break;
            --start;
        }
        return line.substr(start, cursor - start);
    }

    static void trim_in_place(std::string& line) {
        size_t first = 0;
        while (first < line.size() && std::isspace(static_cast<unsigned char>(line[first]))) ++first;
        size_t last = line.size();
        while (last > first && std::isspace(static_cast<unsigned char>(line[last - 1]))) --last;
        line = line.substr(first, last - first);
    }
};

} // namespace qamrpp

#endif
