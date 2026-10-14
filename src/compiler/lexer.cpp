#include "chomsky3/lexer.hpp"
#include "chomsky3/regex.h"
#include <algorithm>
#include <bitset>
#include <map>
#include <memory>
#include <stdexcept>

namespace chomsky3 {
namespace {
struct edge { int to; std::bitset<256> bytes; bool epsilon; };
struct state { std::vector<edge> edges; int accept = -1; };
struct builder {
    std::vector<state> states;
    int fresh() {
        if (states.size() >= 16384) throw std::runtime_error("lexer NFA state limit exceeded");
        states.emplace_back();
        return static_cast<int>(states.size() - 1);
    }
    void epsilon(int a, int b) { states[a].edges.push_back({b, {}, true}); }
    void build(const chomsky3_regex_node_t* n, int a, int b) {
        if (!n) { epsilon(a, b); return; }
        switch (n->type) {
        case CHOMSKY3_NODE_CONCAT: {
            int mid = fresh(); build(n->left, a, mid); build(n->right, mid, b); return;
        }
        case CHOMSKY3_NODE_ALTERNATION: {
            int left_start = fresh(), left_end = fresh();
            int right_start = fresh(), right_end = fresh();
            epsilon(a, left_start); epsilon(a, right_start);
            build(n->left, left_start, left_end); build(n->right, right_start, right_end);
            epsilon(left_end, b); epsilon(right_end, b); return;
        }
        case CHOMSKY3_NODE_GROUP: build(n->left, a, b); return;
        case CHOMSKY3_NODE_STAR:
        case CHOMSKY3_NODE_PLUS:
        case CHOMSKY3_NODE_QUESTION:
        case CHOMSKY3_NODE_REPEAT: {
            unsigned min = 0, max = UINT32_MAX;
            if (n->type == CHOMSKY3_NODE_PLUS) min = 1;
            if (n->type == CHOMSKY3_NODE_QUESTION) max = 1;
            if (n->type == CHOMSKY3_NODE_REPEAT) { min = n->data.repeat.min; max = n->data.repeat.max; }
            if (min > 4096 || (max != UINT32_MAX && max > 4096))
                throw std::runtime_error("lexer repetition limit exceeded");
            int current = a;
            for (unsigned i = 0; i < min; ++i) {
                int next = fresh(); build(n->left, current, next); current = next;
            }
            epsilon(current, b);
            if (max == UINT32_MAX) {
                int next = fresh(); build(n->left, current, next); epsilon(next, current);
            } else {
                for (unsigned i = min; i < max; ++i) {
                    int next = fresh(); build(n->left, current, next); current = next; epsilon(current, b);
                }
            }
            return;
        }
        case CHOMSKY3_NODE_LITERAL:
        case CHOMSKY3_NODE_CHAR_CLASS:
        case CHOMSKY3_NODE_DOT: {
            std::bitset<256> bytes;
            if (n->type == CHOMSKY3_NODE_LITERAL) {
                if (n->data.literal > 255) throw std::runtime_error("lexer requires byte literals");
                bytes.set(n->data.literal);
            } else if (n->type == CHOMSKY3_NODE_DOT) {
                bytes.set(); bytes.reset('\n');
            } else {
                const auto& cc = n->data.char_class;
                for (std::size_t i = 0; i < cc.num_ranges; ++i) {
                    if (cc.ranges[i].end > 255) throw std::runtime_error("lexer requires byte character classes");
                    for (unsigned c = cc.ranges[i].start; c <= cc.ranges[i].end; ++c) bytes.set(c);
                }
                for (unsigned c = 0; c < 256; ++c) {
                    if (cc.bitmap && (cc.bitmap[c / 8] & (1u << (c % 8)))) bytes.set(c);
                    if ((cc.flags & CHOMSKY3_CCLASS_DIGIT) && c >= '0' && c <= '9') bytes.set(c);
                    if ((cc.flags & CHOMSKY3_CCLASS_WORD) && ((c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) bytes.set(c);
                    if ((cc.flags & CHOMSKY3_CCLASS_SPACE) && (c == ' ' || (c >= 9 && c <= 13))) bytes.set(c);
                }
                if (cc.flags & CHOMSKY3_CCLASS_NEGATED) bytes.flip();
            }
            states[a].edges.push_back({b, bytes, false}); return;
        }
        default: throw std::runtime_error("assertions and backreferences are not supported in lexer rules");
        }
    }
    std::vector<int> closure(const std::vector<int>& seeds) const {
        std::vector<bool> seen(states.size());
        std::vector<int> result;
        for (int s : seeds) if (!seen[s]) { seen[s] = true; result.push_back(s); }
        for (std::size_t i = 0; i < result.size(); ++i)
            for (const auto& e : states[result[i]].edges)
                if (e.epsilon && !seen[e.to]) { seen[e.to] = true; result.push_back(e.to); }
        std::sort(result.begin(), result.end());
        return result;
    }
};
}
lexer_dfa compile_lexer(const std::vector<std::string>& patterns, std::size_t max_states) {
    if (patterns.empty() || !max_states) throw std::runtime_error("lexer needs rules and a positive state limit");
    builder nfa;
    int start = nfa.fresh();
    for (std::size_t i = 0; i < patterns.size(); ++i) {
        std::unique_ptr<chomsky3_regex_t, decltype(&chomsky3_regex_free)> regex(
            chomsky3_regex_new(patterns[i].data(), patterns[i].size()), chomsky3_regex_free);
        if (!regex) throw std::bad_alloc();
        if (regex->parse_error != CHOMSKY3_OK)
            throw std::runtime_error("invalid regex in rule " + std::to_string(i + 1) +
                " at byte " + std::to_string(regex->parse_error_pos));
        int a = nfa.fresh(), b = nfa.fresh();
        nfa.epsilon(start, a); nfa.build(regex->root, a, b); nfa.states[b].accept = static_cast<int>(i);
    }
    lexer_dfa dfa;
    std::vector<std::vector<int>> subsets;
    std::map<std::vector<int>, int> ids;
    auto intern = [&](const std::vector<int>& set) {
        auto found = ids.find(set);
        if (found != ids.end()) return found->second;
        if (subsets.size() >= max_states) throw std::runtime_error("lexer DFA state limit exceeded");
        int id = static_cast<int>(subsets.size());
        ids.emplace(set, id); subsets.push_back(set);
        std::array<int, 256> row; row.fill(-1); dfa.transitions.push_back(row);
        int accept = -1;
        for (int s : set) if (nfa.states[s].accept >= 0 && (accept < 0 || nfa.states[s].accept < accept))
            accept = nfa.states[s].accept;
        dfa.accepts.push_back(accept);
        return id;
    };
    intern(nfa.closure({start}));
    if (dfa.accepts[0] >= 0) throw std::runtime_error("lexer rules must not match an empty string");
    for (std::size_t i = 0; i < subsets.size(); ++i) {
        auto set = subsets[i]; // interning can reallocate subsets
        for (unsigned c = 0; c < 256; ++c) {
            std::vector<int> next;
            for (int s : set) for (const auto& e : nfa.states[s].edges)
                if (!e.epsilon && e.bytes[c]) next.push_back(e.to);
            if (!next.empty()) {
                int target = intern(nfa.closure(next));
                dfa.transitions[i][c] = target;
            }
        }
    }
    return dfa;
}
}
