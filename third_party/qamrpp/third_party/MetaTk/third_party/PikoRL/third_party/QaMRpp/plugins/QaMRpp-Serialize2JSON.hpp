#ifndef QAMRPP_SERIALIZE_2_JSON_HPP
#define QAMRPP_SERIALIZE_2_JSON_HPP

#include <string>

#include "../QaMRpp.hpp"

namespace qamrpp {

class Serialize2JSONPlugin : public Plugin {
public:
    const char* name() const { return "Serialize2JSON"; }

    void install(Context& ctx) {
        ctx.register_native("serialize_program", [](Context&, std::vector<ValuePtr>&) -> ValuePtr {
            return std::make_shared<Value>(std::string("{\"status\":\"ok\",\"note\":\"runtime serialization hook\"}"));
        });

        ctx.add_hook(HookPoint::AfterParse, [this](Context&, const HookPayload& payload) {
            if (!payload.ast) return;
            last_json_ = "{\"stage\":\"parsed\",\"ast\":" + node_to_json(payload.ast) + "}";
        });

        ctx.add_hook(HookPoint::OnError, [this](Context&, const HookPayload& payload) {
            std::string msg = payload.error_message ? payload.error_message : "unknown";
            last_json_ = "{\"stage\":\"error\",\"message\":\"" + escape(msg) + "\"}";
        });

        ctx.register_native("get_serialized_program", [this](Context&, std::vector<ValuePtr>&) -> ValuePtr {
            return std::make_shared<Value>(last_json_);
        });
    }

private:
    std::string last_json_ = "{}";

    static std::string escape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '"') out += "\\\"";
            else if (s[i] == '\\') out += "\\\\";
            else if (s[i] == '\n') out += "\\n";
            else out += s[i];
        }
        return out;
    }

    static const char* tname(Node::Type t) {
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

    static std::string node_to_json(const Node* n) {
        std::string out = "{\"type\":\"" + std::string(tname(n->type)) + "\",\"text\":\"" + escape(n->text) + "\",\"children\":[";
        for (size_t i = 0; i < n->children.size(); ++i) {
            if (i) out += ",";
            out += node_to_json(n->children[i].get());
        }
        out += "]}";
        return out;
    }
};

} // namespace qamrpp

#endif
