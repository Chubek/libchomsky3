#pragma once

#include "SerdeTk.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace serdetk::xpath {

enum class ItemKind { Null, Boolean, Integer, Unsigned, Double, String, Binary, Map, Array };

struct Item {
    Value value {};
    std::string name {};
    bool attribute {false};

    [[nodiscard]] ItemKind kind() const noexcept;
    [[nodiscard]] bool is_atomic() const noexcept;
    [[nodiscard]] std::string string_value() const;
};

using Sequence = std::vector<Item>;

struct DynamicContext {
    const Document* document {nullptr};
    Sequence focus {};
    std::size_t position {1};
    std::size_t size {1};
};

[[nodiscard]] Item make_item(Value value, std::string name = {}, bool attribute = false);
[[nodiscard]] bool effective_boolean_value(const Sequence& sequence);
[[nodiscard]] bool atomically_equal(const Item& left, const Item& right);
[[nodiscard]] double numeric_value(const Item& item);

} // namespace serdetk::xpath
