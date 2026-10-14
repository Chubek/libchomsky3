#pragma once

#include "DataModel.hpp"

#include <string_view>

namespace serdetk::xpath {

class Adapter {
public:
    virtual ~Adapter() = default;
    [[nodiscard]] virtual Sequence root(const Document& document) const = 0;
    [[nodiscard]] virtual Sequence child(const Sequence& input, std::string_view name, bool attributes_only = false) const = 0;
    [[nodiscard]] virtual Sequence descendants(const Sequence& input, std::string_view name) const = 0;
    [[nodiscard]] virtual Sequence lookup(const Sequence& input, std::string_view key) const = 0;
};

class SerdeTkAdapter final : public Adapter {
public:
    [[nodiscard]] Sequence root(const Document& document) const override;
    [[nodiscard]] Sequence child(const Sequence& input, std::string_view name, bool attributes_only = false) const override;
    [[nodiscard]] Sequence descendants(const Sequence& input, std::string_view name) const override;
    [[nodiscard]] Sequence lookup(const Sequence& input, std::string_view key) const override;
};

} // namespace serdetk::xpath
