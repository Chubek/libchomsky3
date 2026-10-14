/**
 * @file EkippX-PluginAPI.hpp
 * @brief Static plugin registration API for extending EkippX contexts.
 *
 * The plugin API separates descriptor metadata, compatibility checks, capability
 * negotiation, atomic handlers, hook handlers, and runtime registration. Plugins
 * receive a Registrar and PluginContext during initialization and never own the
 * host Context. This keeps lifecycle, diagnostics, and policy enforcement in the
 * embedding application.
 *
 * @section ekippx_plugin_errors Error behavior
 * Registration conflicts and rejected capabilities throw PluginError-derived
 * exceptions. Compatibility reports should be used for user-facing load errors.
 */
#pragma once

#include "EkippX.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ekippx::plugin {

struct PluginDescriptor;
struct PluginContext;
struct HostInfo;
struct CompatibilityReport;

CompatibilityReport check_plugin_compatibility(const PluginDescriptor& descriptor, const HostInfo& host);
HostInfo host_info(const ekippx::Context& ctx);
PluginContext make_plugin_context(const HostInfo& host, StringView plugin_name);

using String = std::string;
using StringView = std::string_view;
using Path = std::filesystem::path;
using VersionString = std::string;
using CapabilitySet = std::unordered_set<std::string>;
using PropertyMap = std::unordered_map<std::string, ekippx::Value>;

enum class PluginKind { static_plugin, dynamic_plugin, builtin_plugin, meta_plugin };
enum class PluginState { discovered, registered, loaded, initialized, active, suspended, unloaded, failed };
enum class CapabilityLevel { optional, recommended, required };
enum class HookEvent {
  before_lex,
  after_lex,
  before_parse,
  after_parse,
  before_expand,
  after_expand,
  before_include,
  after_include,
  before_directive,
  after_directive,
  before_function,
  after_function,
  before_expander,
  after_expander,
  before_emit,
  after_emit,
  on_error,
  on_warning,
  on_reset,
  on_freeze,
  on_thaw,
};
enum class AtomicKind { lexical, runtime, control, output, plugin_defined };
enum class PluginLoadPolicy { manual, autoload, require_explicit, disabled };
enum class CompatibilityResult { compatible, version_mismatch, missing_capability, host_rejected, plugin_rejected };

class PluginError : public ekippx::Error { public: using Error::Error; };
class PluginLoadError : public PluginError { public: using PluginError::PluginError; };
class PluginInitError : public PluginError { public: using PluginError::PluginError; };
class PluginCapabilityError : public PluginError { public: using PluginError::PluginError; };
class PluginRegistrationError : public PluginError { public: using PluginError::PluginError; };
class HookError : public PluginError { public: using PluginError::PluginError; };

struct Version {
  std::uint32_t major{0};
  std::uint32_t minor{1};
  std::uint32_t patch{0};
  String tag{};
};

struct Capability {
  String name{};
  CapabilityLevel level{CapabilityLevel::optional};
  String description{};
};

struct HostInfo {
  String product_name{"EkippX"};
  Version version{};
  Version api_version{};
  String build_id{"dev"};
  CapabilitySet capabilities{
      "core.directives",
      "core.functions",
      "core.expanders",
      "core.conditionals",
      "core.atomics",
      "runtime.hooks",
      "io.filesystem"};
};

struct PluginInfo {
  String name{};
  String display_name{};
  String description{};
  String author{};
  String license{"repository-default"};
  Version version{};
  Version api_version{};
  PluginKind kind{PluginKind::static_plugin};
  String homepage{};
  std::vector<String> tags{};
};

struct PluginDependency {
  String name{};
  String version_constraint{};
  bool optional{false};
};

struct CompatibilityReport {
  CompatibilityResult result{CompatibilityResult::compatible};
  String message{"compatible"};
  std::vector<String> missing_capabilities{};
};

struct PluginDescriptor {
  PluginInfo info{};
  std::vector<PluginDependency> dependencies{};
  std::vector<Capability> required_capabilities{};
  std::vector<Capability> optional_capabilities{};
  PropertyMap properties{};
};

struct AtomicSpec {
  String name{};
  AtomicKind kind{AtomicKind::plugin_defined};
  String description{};
  bool consumes_newline{false};
  bool requires_runtime{true};
};

struct HookSpec {
  HookEvent event{HookEvent::before_expand};
  int priority{0};
  String label{};
};

struct PluginContext {
  HostInfo host_info{};
  String plugin_name{};
};

struct PluginLoadRequest {
  Path path{};
  PluginLoadPolicy policy{PluginLoadPolicy::manual};
  PropertyMap config{};
};

struct PluginLoadResult {
  bool success{false};
  std::optional<PluginDescriptor> descriptor{};
  String message{};
};

using PluginInitFn = std::function<void(class Registrar&, PluginContext&)>;
using PluginShutdownFn = std::function<void(PluginContext&)>;
using AtomicHandler = std::function<void(ekippx::Context&, StringView)>;
using HookHandler = std::function<void(ekippx::Context&, HookEvent)>;
using CompatibilityFn = std::function<CompatibilityReport(const HostInfo&)>;

class AtomicRegistry {
public:
  void register_atomic(AtomicSpec spec, AtomicHandler handler) {
    const auto key = spec.name;
    specs_[key] = std::move(spec);
    handlers_[key] = std::move(handler);
  }

  void unregister_atomic(StringView name) {
    specs_.erase(std::string(name));
    handlers_.erase(std::string(name));
  }

  [[nodiscard]] bool contains(StringView name) const { return specs_.contains(std::string(name)); }
  [[nodiscard]] const AtomicSpec* find(StringView name) const {
    const auto found = specs_.find(std::string(name));
    return found == specs_.end() ? nullptr : &found->second;
  }
  [[nodiscard]] const AtomicHandler* handler(StringView name) const {
    const auto found = handlers_.find(std::string(name));
    return found == handlers_.end() ? nullptr : &found->second;
  }
  [[nodiscard]] std::vector<String> names() const {
    std::vector<String> result;
    for (const auto& [name, spec] : specs_) {
      (void)spec;
      result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
  }

private:
  std::unordered_map<std::string, AtomicSpec> specs_{};
  std::unordered_map<std::string, AtomicHandler> handlers_{};
};

class HookRegistry {
public:
  void register_hook(HookSpec spec, HookHandler handler) {
    entries_[spec.event].push_back({std::move(spec), std::move(handler)});
    auto& event_entries = entries_[spec.event];
    std::stable_sort(event_entries.begin(), event_entries.end(), [](const auto& left, const auto& right) {
      return left.first.priority < right.first.priority;
    });
  }

  void unregister_hooks(HookEvent event, StringView label = {}) {
    auto found = entries_.find(event);
    if (found == entries_.end()) return;
    if (label.empty()) {
      entries_.erase(found);
      return;
    }
    auto& values = found->second;
    values.erase(std::remove_if(values.begin(), values.end(), [&](const auto& item) {
      return item.first.label == label;
    }), values.end());
  }

  [[nodiscard]] bool has_hooks(HookEvent event) const {
    return entries_.contains(event) && !entries_.at(event).empty();
  }

  [[nodiscard]] std::size_t count(HookEvent event) const {
    const auto found = entries_.find(event);
    return found == entries_.end() ? 0U : found->second.size();
  }

  void dispatch(ekippx::Context& ctx, HookEvent event) const {
    const auto found = entries_.find(event);
    if (found == entries_.end()) return;
    for (const auto& [spec, handler] : found->second) {
      (void)spec;
      handler(ctx, event);
    }
  }

private:
  std::unordered_map<HookEvent, std::vector<std::pair<HookSpec, HookHandler>>> entries_{};
};

class Registrar {
public:
  Registrar(ekippx::Context& host, PluginContext& plugin_context, AtomicRegistry& atomic_registry, HookRegistry& hook_registry)
      : host_(host), plugin_context_(plugin_context), atomic_registry_(atomic_registry), hook_registry_(hook_registry) {}

  [[nodiscard]] ekippx::Context& host() { return host_; }
  [[nodiscard]] PluginContext& plugin_context() { return plugin_context_; }

  void register_directive(ekippx::DirectiveSpec spec, ekippx::DirectiveHandler handler) { host_.register_directive(std::move(spec), std::move(handler)); }
  void register_function(ekippx::FunctionSpec spec, ekippx::FunctionHandler handler) { host_.register_function(std::move(spec), std::move(handler)); }
  void register_expander(ekippx::ExpanderSpec spec, ekippx::ExpanderHandler handler) { host_.register_expander(std::move(spec), std::move(handler)); }
  void register_conditional(ekippx::ConditionalSpec spec, ekippx::ConditionalHandler handler) { host_.register_conditional(std::move(spec), std::move(handler)); }
  void register_atomic(AtomicSpec spec, AtomicHandler handler) { atomic_registry_.register_atomic(std::move(spec), std::move(handler)); }
  void register_hook(HookSpec spec, HookHandler handler) { hook_registry_.register_hook(std::move(spec), std::move(handler)); }
  void define_macro(StringView name, StringView body) { host_.macros().define_object(name, body); host_.symbols().register_macro_name(name); }
  void define_lambda_macro(StringView name, ekippx::MacroLambda fn) { host_.macros().define_lambda(name, std::move(fn)); host_.symbols().register_macro_name(name); }
  void set_metadata(StringView key, const ekippx::Value& value) { host_.set_metadata(key, value); }
  void require_capability(StringView capability_name) {
    if (!plugin_context_.host_info.capabilities.contains(std::string(capability_name))) {
      throw PluginCapabilityError("missing capability: " + std::string(capability_name));
    }
  }
  void export_symbol(StringView name, StringView value) { host_.define_symbol(name, value); }

private:
  ekippx::Context& host_;
  PluginContext& plugin_context_;
  AtomicRegistry& atomic_registry_;
  HookRegistry& hook_registry_;
};

class Plugin {
public:
  virtual ~Plugin() = default;
  [[nodiscard]] virtual PluginDescriptor descriptor() const = 0;
  [[nodiscard]] virtual CompatibilityReport check_compatibility(const HostInfo& host) const {
    return check_plugin_compatibility(descriptor(), host);
  }
  virtual void initialize(Registrar& registrar, PluginContext& ctx) = 0;
  virtual void shutdown(PluginContext& ctx) { (void)ctx; }
};

struct StaticPlugin {
  PluginDescriptor descriptor{};
  std::optional<CompatibilityFn> compatibility{};
  PluginInitFn initialize{};
  std::optional<PluginShutdownFn> shutdown{};
};

class PluginManager {
public:
  explicit PluginManager(ekippx::Context& host) : host_(&host) {}

  void register_static(StaticPlugin plugin) {
    if (plugin.descriptor.info.name.empty()) throw PluginRegistrationError("plugin name must not be empty");
    const auto name = plugin.descriptor.info.name;
    static_plugins_[name] = std::move(plugin);
    states_[name] = PluginState::registered;
    host_->register_plugin_loader(name, [this, name](ekippx::Context&) {
      activate_static(name);
    });
  }

  void register_object(std::shared_ptr<Plugin> plugin) {
    if (!plugin) throw PluginRegistrationError("plugin must not be null");
    const auto descriptor = plugin->descriptor();
    object_plugins_[descriptor.info.name] = std::move(plugin);
    descriptors_[descriptor.info.name] = descriptor;
    states_[descriptor.info.name] = PluginState::registered;
    host_->register_plugin_loader(descriptor.info.name, [this, name = descriptor.info.name](ekippx::Context&) {
      activate_object(name);
    });
  }

  PluginLoadResult load(const PluginLoadRequest& request) {
    PluginLoadResult result;
    if (request.policy == PluginLoadPolicy::disabled || load_policy_ == PluginLoadPolicy::disabled) {
      result.message = "dynamic loading is disabled in pass 1";
      return result;
    }
    result.message = "dynamic loading is not implemented in pass 1";
    return result;
  }

  void unload(StringView name) {
    const std::string key(name);
    states_[key] = PluginState::unloaded;
  }

  [[nodiscard]] bool is_loaded(StringView name) const {
    const auto found = states_.find(std::string(name));
    return found != states_.end() && found->second == PluginState::active;
  }

  [[nodiscard]] PluginState state_of(StringView name) const {
    const auto found = states_.find(std::string(name));
    return found == states_.end() ? PluginState::failed : found->second;
  }

  [[nodiscard]] std::vector<String> loaded_plugins() const {
    std::vector<String> result;
    for (const auto& [name, state] : states_) {
      if (state == PluginState::active) result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
  }

  [[nodiscard]] const PluginDescriptor* descriptor_of(StringView name) const {
    const auto key = std::string(name);
    if (const auto found = descriptors_.find(key); found != descriptors_.end()) return &found->second;
    if (const auto found = static_plugins_.find(key); found != static_plugins_.end()) return &found->second.descriptor;
    if (const auto found = object_plugins_.find(key); found != object_plugins_.end()) {
      object_cache_ = found->second->descriptor();
      return &*object_cache_;
    }
    return nullptr;
  }

  [[nodiscard]] CompatibilityReport check_compatibility(const PluginDescriptor& desc) const {
    return check_plugin_compatibility(desc, host_info(*host_));
  }

  void add_search_path(const Path& path) { search_paths_.push_back(path); }
  void clear_search_paths() { search_paths_.clear(); }
  void set_load_policy(PluginLoadPolicy policy) { load_policy_ = policy; }
  [[nodiscard]] PluginLoadPolicy load_policy() const { return load_policy_; }
  [[nodiscard]] AtomicRegistry& atomics() { return atomics_; }
  [[nodiscard]] HookRegistry& hooks() { return hooks_; }

private:
  void activate_static(const std::string& name) {
    auto found = static_plugins_.find(name);
    if (found == static_plugins_.end()) throw PluginLoadError("unknown static plugin: " + name);
    auto plugin_context = make_plugin_context(host_info(*host_), name);
    Registrar registrar(*host_, plugin_context, atomics_, hooks_);
    const auto report = found->second.compatibility ? (*found->second.compatibility)(plugin_context.host_info)
                                                    : check_plugin_compatibility(found->second.descriptor, plugin_context.host_info);
    if (report.result != CompatibilityResult::compatible) throw PluginCapabilityError(report.message);
    found->second.initialize(registrar, plugin_context);
    descriptors_[name] = found->second.descriptor;
    states_[name] = PluginState::active;
  }

  void activate_object(const std::string& name) {
    auto found = object_plugins_.find(name);
    if (found == object_plugins_.end()) throw PluginLoadError("unknown object plugin: " + name);
    auto plugin_context = make_plugin_context(host_info(*host_), name);
    Registrar registrar(*host_, plugin_context, atomics_, hooks_);
    const auto report = found->second->check_compatibility(plugin_context.host_info);
    if (report.result != CompatibilityResult::compatible) throw PluginCapabilityError(report.message);
    found->second->initialize(registrar, plugin_context);
    descriptors_[name] = found->second->descriptor();
    states_[name] = PluginState::active;
  }

  ekippx::Context* host_;
  std::unordered_map<std::string, StaticPlugin> static_plugins_{};
  std::unordered_map<std::string, std::shared_ptr<Plugin>> object_plugins_{};
  std::unordered_map<std::string, PluginDescriptor> descriptors_{};
  mutable std::optional<PluginDescriptor> object_cache_{};
  std::unordered_map<std::string, PluginState> states_{};
  std::vector<Path> search_paths_{};
  PluginLoadPolicy load_policy_{PluginLoadPolicy::manual};
  AtomicRegistry atomics_{};
  HookRegistry hooks_{};
};

inline HostInfo host_info(const ekippx::Context& ctx) {
  HostInfo info;
  if (ctx.config().runtime.shell_policy == ShellPolicy::allow) info.capabilities.insert("runtime.shell");
  return info;
}

inline PluginContext make_plugin_context(const HostInfo& host, StringView plugin_name) {
  return PluginContext{host, std::string(plugin_name)};
}

inline StaticPlugin make_static_plugin(
    PluginDescriptor descriptor,
    PluginInitFn initialize,
    std::optional<PluginShutdownFn> shutdown = std::nullopt) {
  return StaticPlugin{std::move(descriptor), std::nullopt, std::move(initialize), std::move(shutdown)};
}

inline CompatibilityReport check_plugin_compatibility(const PluginDescriptor& descriptor, const HostInfo& host) {
  CompatibilityReport report;
  report.result = CompatibilityResult::compatible;
  report.message = "compatible";
  for (const auto& capability : descriptor.required_capabilities) {
    if (!host.capabilities.contains(capability.name)) {
      report.result = CompatibilityResult::missing_capability;
      report.missing_capabilities.push_back(capability.name);
    }
  }
  if (!report.missing_capabilities.empty()) {
    report.message = "missing required capabilities";
  }
  return report;
}

}  // namespace ekippx::plugin
