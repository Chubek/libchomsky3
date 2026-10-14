#include "Jiterati.hpp"

#include <iostream>

int main() {
    jiterati::Module module("api_jbl_roundtrip_details");
    auto* identity = module.create_function<int(int)>("identity");
    auto* identity_entry = identity->create_block("entry");
    identity_entry->ret(identity_entry->arg(0));

    auto* biased = module.create_function<int(int)>("biased_identity");
    auto* biased_entry = biased->create_block("entry");
    biased_entry->ret(biased_entry->add(biased_entry->arg(0), biased->const_i32(5)));

    std::string serialized = jiterati::print_jbl(module);
    std::string error;
    auto parsed = jiterati::parse_jbl(serialized, &error);
    if (!parsed) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << serialized;
    std::cout << jiterati::print_jbl(*parsed);
    return parsed->find_function("identity") == nullptr ? 1 : 0;
}
