#ifndef QAMRPP_DUMP_HPP
#define QAMRPP_DUMP_HPP

#include <fstream>
#include <sstream>
#include <string>

#include "../QaMRpp.hpp"

namespace qamrpp {

class DumpPlugin : public Plugin {
public:
    const char* name() const { return "Dump"; }

    void install(Context& ctx) {
        ctx.register_native("set_dump_file", [this](Context&, std::vector<ValuePtr>& args) -> ValuePtr {
            if (!args.empty()) dump_file_ = args[0]->to_string();
            return std::make_shared<Value>();
        });

        ctx.add_hook(HookPoint::AfterParse, [this](Context&, const HookPayload& payload) {
            if (!payload.ast || dump_file_.empty()) return;
            std::ofstream out(dump_file_.c_str(), std::ios::out | std::ios::trunc);
            if (!out.good()) return;
            int next_id = 0;
            out << "digraph qamrpp_ast {\n";
            dump_node(out, payload.ast, next_id, -1);
            out << "}\n";
        });
    }

private:
    std::string dump_file_;

    static const char* node_type_name(Node::Type t) {
        switch (t) {
            case Node::NODE_LITERAL: return "literal";
            case Node::NODE_IDENT: return "ident";
            case Node::NODE_BINARY: return "binary";
            case Node::NODE_CALL: return "call";
            case Node::NODE_ASSIGN: return "assign";
            case Node::NODE_BLOCK: return "block";
        }
        return "unknown";
    }

    static std::string escape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '"') out += "\\\"";
            else if (s[i] == '\\') out += "\\\\";
            else out += s[i];
        }
        return out;
    }

    static int dump_node(std::ofstream& out, const Node* n, int& next_id, int parent) {
        const int my_id = next_id++;
        std::string label = node_type_name(n->type);
        if (!n->text.empty()) label += "\\n" + escape(n->text);
        out << "  n" << my_id << " [label=\"" << label << "\"];\n";
        if (parent >= 0) out << "  n" << parent << " -> n" << my_id << ";\n";
        for (size_t i = 0; i < n->children.size(); ++i) {
            dump_node(out, n->children[i].get(), next_id, my_id);
        }
        return my_id;
    }
};

} // namespace qamrpp

#endif
