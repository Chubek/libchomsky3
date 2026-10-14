#include "DepndencyGraph.hpp"

#include <algorithm>
#include <lemon/list_graph.h>
#include <map>
#include <set>

namespace serdetk::semver {
namespace {

void populate_graph(lemon::ListDigraph& graph,
                    std::map<std::string, lemon::ListDigraph::Node, std::less<>>& nodes,
                    const std::map<std::string, std::vector<std::string>, std::less<>>& edges) {
    auto node = [&](const std::string& label) {
        if (const auto iterator = nodes.find(label); iterator != nodes.end()) return iterator->second;
        const auto value = graph.addNode();
        nodes.emplace(label, value);
        return value;
    };
    for (const auto& [source, targets] : edges) {
        const auto source_node = node(source);
        for (const auto& target : targets) graph.addArc(source_node, node(target));
    }
}

} // namespace

void DependencyGraph::add_edge(std::string dependent, std::string dependency) {
    auto& dependencies = edges_[std::move(dependent)];
    if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end())
        dependencies.push_back(std::move(dependency));
}

bool DependencyGraph::contains_cycle() const {
    lemon::ListDigraph graph;
    std::map<std::string, lemon::ListDigraph::Node, std::less<>> nodes;
    populate_graph(graph, nodes, edges_);
    std::set<std::string, std::less<>> visiting;
    std::set<std::string, std::less<>> visited;
    const auto visit = [&](const auto& self, const std::string& label) -> bool {
        if (visiting.contains(label)) return true;
        if (!visited.insert(label).second) return false;
        visiting.insert(label);
        if (const auto iterator = edges_.find(label); iterator != edges_.end())
            for (const auto& dependency : iterator->second)
                if (self(self, dependency)) return true;
        visiting.erase(label);
        return false;
    };
    for (const auto& [label, _] : nodes)
        if (visit(visit, label)) return true;
    return false;
}

std::vector<std::string> DependencyGraph::topological_order() const {
    if (contains_cycle()) return {};
    lemon::ListDigraph graph;
    std::map<std::string, lemon::ListDigraph::Node, std::less<>> nodes;
    populate_graph(graph, nodes, edges_);
    std::vector<std::string> result;
    std::set<std::string, std::less<>> visited;
    const auto visit = [&](const auto& self, const std::string& label) -> void {
        if (!visited.insert(label).second) return;
        if (const auto iterator = edges_.find(label); iterator != edges_.end())
            for (const auto& dependency : iterator->second) self(self, dependency);
        result.push_back(label);
    };
    for (const auto& [label, _] : nodes) visit(visit, label);
    return result;
}

} // namespace serdetk::semver
