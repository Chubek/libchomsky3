#include "Adapter.hpp"

#include <charconv>

namespace serdetk::xpath {
namespace {

void append_children(const Item& item, std::string_view name, bool attributes_only, Sequence& out) {
    if (item.value.is_object()) {
        for (const auto& [key, value] : item.value.as_object().fields) {
            const bool attribute = !key.empty() && key.front() == '@';
            if (attributes_only != attribute) continue;
            const auto bare_name = attribute ? std::string_view(key).substr(1) : std::string_view(key);
            if (name == "*" || name == bare_name) out.push_back(make_item(value, std::string(bare_name), attribute));
        }
    } else if (item.value.is_array() && !attributes_only) {
        const auto& values = item.value.as_array().items;
        for (std::size_t index = 0; index < values.size(); ++index)
            if (name == "*" || name == std::to_string(index + 1))
                out.push_back(make_item(values[index], std::to_string(index + 1)));
    }
}

void append_descendants(const Item& item, std::string_view name, Sequence& out) {
    Sequence children;
    append_children(item, "*", false, children);
    for (const auto& child : children) {
        if (name == "*" || child.name == name) out.push_back(child);
        append_descendants(child, name, out);
    }
}

} // namespace

Sequence SerdeTkAdapter::root(const Document& document) const {
    return {make_item(document.root, "root")};
}

Sequence SerdeTkAdapter::child(const Sequence& input, std::string_view name, bool attributes_only) const {
    Sequence out;
    for (const auto& item : input) append_children(item, name, attributes_only, out);
    return out;
}

Sequence SerdeTkAdapter::descendants(const Sequence& input, std::string_view name) const {
    Sequence out;
    for (const auto& item : input) append_descendants(item, name, out);
    return out;
}

Sequence SerdeTkAdapter::lookup(const Sequence& input, std::string_view key) const {
    Sequence out;
    for (const auto& item : input) {
        if (item.value.is_object()) {
            const auto& fields = item.value.as_object().fields;
            if (key == "*") {
                for (const auto& [name, value] : fields) out.push_back(make_item(value, name));
            } else if (const auto found = fields.find(std::string(key)); found != fields.end()) {
                out.push_back(make_item(found->second, found->first));
            }
        } else if (item.value.is_array()) {
            const auto& values = item.value.as_array().items;
            if (key == "*") {
                for (std::size_t index = 0; index < values.size(); ++index)
                    out.push_back(make_item(values[index], std::to_string(index + 1)));
            } else {
                std::size_t index {};
                const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index);
                if (error == std::errc{} && end == key.data() + key.size() && index > 0 && index <= values.size())
                    out.push_back(make_item(values[index - 1], std::to_string(index)));
            }
        }
    }
    return out;
}

} // namespace serdetk::xpath
