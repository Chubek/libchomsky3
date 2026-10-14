#include "DataModel.hpp"

#include <cmath>
#include <sstream>

namespace serdetk::xpath {

ItemKind Item::kind() const noexcept {
    if (value.is_null()) return ItemKind::Null;
    if (value.is_bool()) return ItemKind::Boolean;
    if (value.is_int()) return ItemKind::Integer;
    if (value.is_uint()) return ItemKind::Unsigned;
    if (value.is_double()) return ItemKind::Double;
    if (value.is_string()) return ItemKind::String;
    if (value.is_binary()) return ItemKind::Binary;
    if (value.is_object()) return ItemKind::Map;
    return ItemKind::Array;
}

bool Item::is_atomic() const noexcept {
    return !value.is_object() && !value.is_array();
}

std::string Item::string_value() const {
    if (value.is_null()) return {};
    if (value.is_bool()) return std::get<bool>(value.data) ? "true" : "false";
    if (value.is_int()) return std::to_string(std::get<std::int64_t>(value.data));
    if (value.is_uint()) return std::to_string(std::get<std::uint64_t>(value.data));
    if (value.is_double()) {
        std::ostringstream out;
        out << std::get<double>(value.data);
        return out.str();
    }
    if (value.is_string()) return value.as_string();
    if (value.is_binary()) return std::string(value.as_binary().bytes.begin(), value.as_binary().bytes.end());
    return {};
}

Item make_item(Value value, std::string name, bool attribute) {
    return Item{std::move(value), std::move(name), attribute};
}

bool effective_boolean_value(const Sequence& sequence) {
    if (sequence.empty()) return false;
    if (sequence.size() != 1) return true;
    const auto& item = sequence.front();
    if (item.value.is_null()) return false;
    if (item.value.is_bool()) return std::get<bool>(item.value.data);
    if (item.value.is_string()) return !item.value.as_string().empty();
    if (item.value.is_int()) return std::get<std::int64_t>(item.value.data) != 0;
    if (item.value.is_uint()) return std::get<std::uint64_t>(item.value.data) != 0;
    if (item.value.is_double()) {
        const auto number = std::get<double>(item.value.data);
        return number != 0.0 && !std::isnan(number);
    }
    return true;
}

double numeric_value(const Item& item) {
    if (item.value.is_int()) return static_cast<double>(std::get<std::int64_t>(item.value.data));
    if (item.value.is_uint()) return static_cast<double>(std::get<std::uint64_t>(item.value.data));
    if (item.value.is_double()) return std::get<double>(item.value.data);
    if (item.value.is_bool()) return std::get<bool>(item.value.data) ? 1.0 : 0.0;
    try {
        return std::stod(item.string_value());
    } catch (...) {
        throw QueryError("err:XPTY0004: numeric operand required");
    }
}

bool atomically_equal(const Item& left, const Item& right) {
    if (!left.is_atomic() || !right.is_atomic())
        throw QueryError("err:XPTY0004: atomic values required for comparison");
    if ((left.value.is_int() || left.value.is_uint() || left.value.is_double()) &&
        (right.value.is_int() || right.value.is_uint() || right.value.is_double()))
        return numeric_value(left) == numeric_value(right);
    if (left.value.is_bool() && right.value.is_bool())
        return std::get<bool>(left.value.data) == std::get<bool>(right.value.data);
    return left.string_value() == right.string_value();
}

} // namespace serdetk::xpath
