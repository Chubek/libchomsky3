#include "Resolver.hpp"

#include <Satie.hpp>

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <utility>

namespace serdetk::semver {
namespace {

using Variable = std::pair<std::string, std::size_t>;

std::vector<std::string> reachable_packages(
    const Storage& storage, const std::map<std::string, Constraint, std::less<>>& roots) {
    std::set<std::string, std::less<>> seen;
    std::deque<std::string> queue;
    for (const auto& [package, _] : roots) queue.push_back(package);
    while (!queue.empty()) {
        std::string package = std::move(queue.front());
        queue.pop_front();
        if (!seen.insert(package).second) continue;
        const auto* releases = storage.find(package);
        if (!releases) continue;
        for (const auto& release : *releases)
            for (const auto& [dependency, _] : release.dependencies) queue.push_back(dependency);
    }
    return {seen.begin(), seen.end()};
}

} // namespace

Resolution Resolver::resolve(const std::map<std::string, Constraint, std::less<>>& requirements) const {
    Resolution resolution;
    const auto packages = reachable_packages(storage_, requirements);
    std::map<Variable, satie::Var> variables;
    satie::CNF formula;
    satie::Var next_variable = 1;

    for (const auto& package : packages) {
        const auto* releases = storage_.find(package);
        if (!releases || releases->empty()) {
            resolution.diagnostics.push_back({package, "no releases are available"});
            return resolution;
        }
        satie::Clause choices;
        for (std::size_t index = 0; index < releases->size(); ++index) {
            variables.emplace(Variable{package, index}, next_variable);
            choices.push_back(next_variable++);
        }
        formula.add_clause(std::move(choices));
        for (std::size_t left = 0; left < releases->size(); ++left)
            for (std::size_t right = left + 1; right < releases->size(); ++right)
                formula.add_clause({-variables.at({package, left}), -variables.at({package, right})});
    }

    for (const auto& [package, constraint] : requirements) {
        const auto* releases = storage_.find(package);
        if (!releases) continue;
        satie::Clause accepted;
        for (std::size_t index = 0; index < releases->size(); ++index)
            if (constraint.matches((*releases)[index].version)) accepted.push_back(variables.at({package, index}));
        if (accepted.empty()) {
            resolution.diagnostics.push_back({package, "no release satisfies root constraint"});
            return resolution;
        }
        formula.add_clause(std::move(accepted));
    }

    for (const auto& package : packages) {
        const auto* releases = storage_.find(package);
        for (std::size_t index = 0; index < releases->size(); ++index) {
            const auto variable = variables.at({package, index});
            for (const auto& [dependency, constraint] : (*releases)[index].dependencies) {
                const auto* dependency_releases = storage_.find(dependency);
                satie::Clause implication{-variable};
                if (dependency_releases) {
                    for (std::size_t dependency_index = 0; dependency_index < dependency_releases->size(); ++dependency_index)
                        if (constraint.matches((*dependency_releases)[dependency_index].version))
                            implication.push_back(variables.at({dependency, dependency_index}));
                }
                if (implication.size() == 1)
                    resolution.diagnostics.push_back({package, "release " + (*releases)[index].version.str() +
                        " requires unsatisfied dependency " + dependency});
                formula.add_clause(std::move(implication));
            }
        }
    }

    const auto report = satie::Solver(std::move(formula)).solve_with_report({satie::Engine::CDCL});
    if (!report.result.satisfiable()) {
        resolution.diagnostics.push_back({"resolver", "constraints are unsatisfiable"});
        return resolution;
    }
    resolution.satisfiable = true;
    for (const auto& package : packages) {
        const auto* releases = storage_.find(package);
        for (std::size_t index = 0; index < releases->size(); ++index) {
            if (report.result.assignment.get_var(variables.at({package, index})) == satie::Value::TRUE) {
                resolution.selected.emplace(package, (*releases)[index]);
                break;
            }
        }
    }
    for (const auto& [package, release] : resolution.selected)
        for (const auto& [dependency, _] : release.dependencies)
            if (resolution.selected.contains(dependency)) resolution.graph.add_edge(package, dependency);
    if (resolution.graph.contains_cycle())
        resolution.diagnostics.push_back({"resolver", "resolved dependency graph contains a cycle"});
    return resolution;
}

} // namespace serdetk::semver
