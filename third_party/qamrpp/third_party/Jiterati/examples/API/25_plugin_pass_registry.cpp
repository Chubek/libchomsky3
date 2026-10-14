#include "Jiterati-Pass.hpp"
#include "Jiterati-Plugin.hpp"

#include <iostream>

class CountModulePass final : public jiterati::ModulePass {
public:
    std::string name() const override { return "count-module"; }

    void run(jiterati::Module& module) override {
        count_ = module.functions().size();
    }

    std::size_t count() const { return count_; }

private:
    std::size_t count_ = 0;
};

int main() {
    jiterati::PluginRegistry::instance().register_plugin("example.registry", {1, 2, 3});
    auto version = jiterati::PluginRegistry::instance().version_of("example.registry");

    jiterati::Module module("api_plugin_pass_registry");
    auto* function = module.create_function<int(int)>("identity");
    auto* entry = function->create_block("entry");
    entry->ret(entry->arg(0));

    jiterati::PassManager pipeline;
    pipeline.add(std::make_unique<CountModulePass>());
    pipeline.run(module);

    std::cout << version.major << '.' << version.minor << '.' << version.patch << '\n';
    std::cout << module.to_string();
    return jiterati::PluginRegistry::instance().has_plugin("example.registry") ? 0 : 1;
}
