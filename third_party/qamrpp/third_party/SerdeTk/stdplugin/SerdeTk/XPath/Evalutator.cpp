#include "Evalutator.hpp"

#include <cmath>
#include <memory>

namespace serdetk::xpath {
namespace {

Sequence boolean_item(bool value) { return {make_item(Value(value))}; }
Sequence number_item(double value) { return {make_item(Value(value))}; }

bool compare(const Item& left, const Item& right, const std::string& operation) {
    if (operation == "=" || operation == "!=") {
        const bool equal = atomically_equal(left, right);
        return operation == "=" ? equal : !equal;
    }
    const auto left_number = numeric_value(left);
    const auto right_number = numeric_value(right);
    if (operation == "<" || operation == "lt") return left_number < right_number;
    if (operation == "<=" || operation == "le") return left_number <= right_number;
    if (operation == ">" || operation == "gt") return left_number > right_number;
    if (operation == ">=" || operation == "ge") return left_number >= right_number;
    throw QueryError("err:XPST0003: unsupported comparison operator");
}

} // namespace

Sequence Evalutator::evaluate(const CodeDAG& dag, const Document& document) const {
    if (!dag.root()) return {};
    DynamicContext context;
    context.document = &document;
    context.focus = adapter_.root(document);
    return evaluate(*dag.root(), std::move(context));
}

Sequence Evalutator::evaluate(const CodeTree& tree, DynamicContext context) const {
    switch (tree.kind) {
    case NodeKind::Literal:
        if (tree.text == "true") return boolean_item(true);
        if (tree.text == "false") return boolean_item(false);
        if (tree.text == "null") return {make_item(Value(nullptr))};
        try {
            std::size_t parsed {};
            const double value = std::stod(tree.text, &parsed);
            if (parsed == tree.text.size()) return number_item(value);
        } catch (...) {}
        return {make_item(Value(tree.text))};
    case NodeKind::ContextItem:
        return context.focus;
    case NodeKind::Root:
        if (!context.document) throw QueryError("err:XPDY0002: absent context document");
        return adapter_.root(*context.document);
    case NodeKind::Path:
        return path(tree, std::move(context));
    case NodeKind::Unary: {
        const auto values = evaluate(*tree.children.front(), context);
        if (values.empty()) return {};
        const double value = numeric_value(values.front());
        return number_item(tree.text == "-" ? -value : value);
    }
    case NodeKind::Binary:
        return binary(tree, std::move(context));
    case NodeKind::Function:
        return function(tree, std::move(context));
    case NodeKind::ArrayConstructor: {
        auto result = std::make_shared<Array>();
        for (const auto& child : tree.children) {
            const auto values = evaluate(*child, context);
            if (values.size() == 1) result->push(values.front().value);
            else {
                auto nested = std::make_shared<Array>();
                for (const auto& value : values) nested->push(value.value);
                result->push(Value(nested));
            }
        }
        return {make_item(Value(result))};
    }
    case NodeKind::MapConstructor: {
        auto result = std::make_shared<Object>();
        for (const auto& [key, value] : tree.entries) {
            const auto keys = evaluate(*key, context);
            const auto values = evaluate(*value, context);
            if (keys.size() != 1 || values.size() != 1) throw QueryError("err:XPTY0004: map entry must be singleton");
            result->set(keys.front().string_value(), values.front().value);
        }
        return {make_item(Value(result))};
    }
    }
    return {};
}

Sequence Evalutator::path(const CodeTree& tree, DynamicContext context) const {
    Sequence current = tree.text == "absolute"
        ? (context.document ? adapter_.root(*context.document) : throw QueryError("err:XPDY0002: absent context document"))
        : context.focus;
    for (const auto& step : tree.steps) {
        switch (step.kind) {
        case StepKind::Child: current = adapter_.child(current, step.name); break;
        case StepKind::Descendant: current = adapter_.descendants(current, step.name); break;
        case StepKind::Wildcard: current = adapter_.child(current, "*"); break;
        case StepKind::Attribute: current = adapter_.child(current, step.name, true); break;
        case StepKind::Lookup: current = adapter_.lookup(current, step.name); break;
        }
        for (const auto& filter : step.predicates) current = predicate(current, *filter, context);
    }
    return current;
}

Sequence Evalutator::predicate(const Sequence& input, const CodeTree& expression, DynamicContext context) const {
    Sequence result;
    for (std::size_t index = 0; index < input.size(); ++index) {
        DynamicContext item_context = context;
        item_context.focus = {input[index]};
        item_context.position = index + 1;
        item_context.size = input.size();
        const auto condition = evaluate(expression, std::move(item_context));
        if (condition.size() == 1 && (condition.front().value.is_int() || condition.front().value.is_uint() || condition.front().value.is_double())) {
            if (static_cast<std::size_t>(numeric_value(condition.front())) == index + 1) result.push_back(input[index]);
        } else if (effective_boolean_value(condition)) result.push_back(input[index]);
    }
    return result;
}

Sequence Evalutator::binary(const CodeTree& tree, DynamicContext context) const {
    const auto left = evaluate(*tree.children[0], context);
    if (tree.text == "and") return boolean_item(effective_boolean_value(left) && effective_boolean_value(evaluate(*tree.children[1], context)));
    if (tree.text == "or") return boolean_item(effective_boolean_value(left) || effective_boolean_value(evaluate(*tree.children[1], context)));
    const auto right = evaluate(*tree.children[1], context);
    if (tree.text == "|") {
        Sequence result = left;
        result.insert(result.end(), right.begin(), right.end());
        return result;
    }
    if (tree.text == "=" || tree.text == "!=" || tree.text == "<" || tree.text == "<=" ||
        tree.text == ">" || tree.text == ">=" || tree.text == "lt" || tree.text == "le" ||
        tree.text == "gt" || tree.text == "ge") {
        for (const auto& lhs : left)
            for (const auto& rhs : right)
                if (compare(lhs, rhs, tree.text)) return boolean_item(true);
        return boolean_item(false);
    }
    if (left.empty() || right.empty()) return {};
    const auto lhs = numeric_value(left.front());
    const auto rhs = numeric_value(right.front());
    if (tree.text == "+") return number_item(lhs + rhs);
    if (tree.text == "-") return number_item(lhs - rhs);
    if (tree.text == "*") return number_item(lhs * rhs);
    if (tree.text == "div") return number_item(lhs / rhs);
    if (tree.text == "idiv") return number_item(std::floor(lhs / rhs));
    if (tree.text == "mod") return number_item(std::fmod(lhs, rhs));
    throw QueryError("err:XPST0003: unsupported binary operator");
}

Sequence Evalutator::function(const CodeTree& tree, DynamicContext context) const {
    const auto argument = [&](std::size_t index) { return evaluate(*tree.children.at(index), context); };
    if (tree.text == "true") return boolean_item(true);
    if (tree.text == "false") return boolean_item(false);
    if (tree.text == "position") return number_item(static_cast<double>(context.position));
    if (tree.text == "last") return number_item(static_cast<double>(context.size));
    if (tree.text == "count") return number_item(static_cast<double>(argument(0).size()));
    if (tree.text == "exists") return boolean_item(!argument(0).empty());
    if (tree.text == "empty") return boolean_item(argument(0).empty());
    if (tree.text == "boolean") return boolean_item(effective_boolean_value(argument(0)));
    if (tree.text == "not") return boolean_item(!effective_boolean_value(argument(0)));
    if (tree.text == "string") {
        const auto values = argument(0);
        return {make_item(Value(values.empty() ? std::string{} : values.front().string_value()))};
    }
    if (tree.text == "name") {
        const auto values = tree.children.empty() ? context.focus : argument(0);
        return {make_item(Value(values.empty() ? std::string{} : values.front().name))};
    }
    throw QueryError("err:XPST0017: unsupported function " + tree.text);
}

} // namespace serdetk::xpath
