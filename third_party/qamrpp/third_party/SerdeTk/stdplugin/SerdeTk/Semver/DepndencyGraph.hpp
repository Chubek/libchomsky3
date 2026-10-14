#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace serdetk::semver {

class DependencyGraph {
public:
    void add_edge(std::string dependent, std::string dependency);
    [[nodiscard]] bool contains_cycle() const;
    [[nodiscard]] std::vector<std::string> topological_order() const;
    [[nodiscard]] const std::map<std::string, std::vector<std::string>, std::less<>>& edges() const noexcept {
        return edges_;
    }

private:
    std::map<std::string, std::vector<std::string>, std::less<>> edges_;
};

} // namespace serdetk::semver
