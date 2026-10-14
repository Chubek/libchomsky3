#ifndef POLYFLOW_HPP
#define POLYFLOW_HPP

#include "Grafitt/Grafitt.hpp"
#include "PolyflowData.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace polyflow {

using task_id = std::uint64_t;

enum class task_state { pending, ready, running, completed, failed, cancelled };
enum class priority : int { critical = 0, high = 1, normal = 2, low = 3, background = 4 };

struct task_metadata {
  std::string name;
  priority prio = priority::normal;
  std::optional<std::chrono::milliseconds> timeout;
};

class task_error : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class scheduling_error : public std::runtime_error { public: using std::runtime_error::runtime_error; };

struct cancellation_token {
  std::atomic<bool> cancelled{false};
  bool is_cancelled() const noexcept { return cancelled.load(std::memory_order_relaxed); }
  void cancel() noexcept { cancelled.store(true, std::memory_order_relaxed); }
};

class task_base : public std::enable_shared_from_this<task_base> {
public:
  using fn_type = std::function<zethamem::Value(cancellation_token&)>;

  task_base(task_id id, task_metadata md, fn_type fn)
      : id_(id), metadata_(std::move(md)), fn_(std::move(fn)), token_(std::make_shared<cancellation_token>()) {}

  task_id id() const noexcept { return id_; }
  const task_metadata& metadata() const noexcept { return metadata_; }
  task_state state() const noexcept { return state_.load(std::memory_order_acquire); }

  void add_dependency(task_id dep) { deps_.push_back(dep); }
  const std::vector<task_id>& dependencies() const noexcept { return deps_; }

  void set_state(task_state next) {
    std::lock_guard<std::mutex> lock(m_);
    if (!legal_transition(state_.load(std::memory_order_relaxed), next)) throw task_error("illegal task state transition");
    state_.store(next, std::memory_order_release);
    cv_.notify_all();
  }

  void cancel() {
    token_->cancel();
    const auto cur = state();
    if (cur == task_state::pending || cur == task_state::ready || cur == task_state::running) set_state(task_state::cancelled);
  }

  template<typename F>
  std::shared_ptr<task_base> then(task_id next_id, task_metadata next_md, F&& cont) {
    auto prev = shared_from_this();
    auto next_fn = [prev, c = std::forward<F>(cont)](cancellation_token& tok) -> zethamem::Value {
      if (tok.is_cancelled()) throw task_error("continuation cancelled");
      auto v = prev->wait();
      return c(v, tok);
    };
    auto child = std::make_shared<task_base>(next_id, std::move(next_md), std::move(next_fn));
    child->add_dependency(id_);
    return child;
  }

  std::optional<zethamem::Value> try_get() const {
    std::lock_guard<std::mutex> lock(m_);
    return value_;
  }

  zethamem::Value wait() const {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock, [&] {
      const auto s = state_.load(std::memory_order_acquire);
      return s == task_state::completed || s == task_state::failed || s == task_state::cancelled;
    });
    if (error_) std::rethrow_exception(error_);
    if (!value_) throw task_error("task completed without value");
    return *value_;
  }

  void run() {
    if (token_->is_cancelled()) {
      set_state(task_state::cancelled);
      return;
    }

    set_state(task_state::running);
    const auto start = std::chrono::steady_clock::now();
    try {
      zethamem::Value out = fn_(*token_);
      if (metadata_.timeout && (std::chrono::steady_clock::now() - start) > *metadata_.timeout) throw task_error("task timeout exceeded");
      {
        std::lock_guard<std::mutex> lock(m_);
        value_ = std::move(out);
      }
      set_state(task_state::completed);
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(m_);
        error_ = std::current_exception();
      }
      set_state(token_->is_cancelled() ? task_state::cancelled : task_state::failed);
    }
  }

private:
  static bool legal_transition(task_state from, task_state to) {
    switch (from) {
      case task_state::pending: return to == task_state::ready || to == task_state::cancelled;
      case task_state::ready: return to == task_state::running || to == task_state::cancelled;
      case task_state::running: return to == task_state::completed || to == task_state::failed || to == task_state::cancelled;
      case task_state::completed:
      case task_state::failed:
      case task_state::cancelled: return false;
    }
    return false;
  }

  task_id id_;
  task_metadata metadata_;
  fn_type fn_;
  std::shared_ptr<cancellation_token> token_;
  std::vector<task_id> deps_;
  std::atomic<task_state> state_{task_state::pending};

  mutable std::mutex m_;
  mutable std::condition_variable cv_;
  std::optional<zethamem::Value> value_;
  std::exception_ptr error_;
};

class task_graph {
public:
  using graph_t = grafitt::imperative_graph<task_id, std::string>;

  void add_task(const std::shared_ptr<task_base>& t) {
    tasks_[t->id()] = t;
    graph_.add_vertex(t->id());
    for (auto dep : t->dependencies()) {
      graph_.add_vertex(dep);
      graph_.add_edge(dep, t->id(), "dep");
    }
    refresh_metadata_row(*t);
  }

  bool has_cycle() const {
    std::unordered_set<task_id> white, gray, black;
    graph_.iter_vertex([&](task_id v) { white.insert(v); });

    std::function<bool(task_id)> dfs = [&](task_id v) {
      white.erase(v);
      gray.insert(v);
      for (auto n : graph_.succ(v)) {
        if (black.contains(n)) continue;
        if (gray.contains(n)) return true;
        if (white.contains(n) && dfs(n)) return true;
      }
      gray.erase(v);
      black.insert(v);
      return false;
    };

    while (!white.empty()) {
      if (dfs(*white.begin())) return true;
    }
    return false;
  }

  std::vector<task_id> schedule() const {
    if (has_cycle()) throw scheduling_error("graph contains cycle");

    std::unordered_map<task_id, std::size_t> indeg;
    graph_.iter_vertex([&](task_id v) { indeg[v] = graph_.pred(v).size(); });

    std::deque<task_id> queue;
    for (const auto& [v, d] : indeg) if (d == 0) queue.push_back(v);

    std::vector<task_id> out;
    while (!queue.empty()) {
      const auto v = queue.front();
      queue.pop_front();
      out.push_back(v);

      for (auto n : graph_.succ(v)) {
        auto& dn = indeg[n];
        if (--dn == 0) queue.push_back(n);
      }
    }

    if (out.size() != indeg.size()) throw scheduling_error("topological sort failed");
    return out;
  }

  const std::unordered_map<task_id, std::shared_ptr<task_base>>& tasks() const noexcept { return tasks_; }
  const graph_t& graph() const noexcept { return graph_; }
  zethamem::Table& metadata_table() noexcept { return metadata_; }

private:
  void refresh_metadata_row(const task_base& t) {
    if (metadata_.schema().columns.empty()) {
      zethamem::Schema schema;
      schema.table_name = "polyflow_task_metadata";
      schema.columns = {
          {"task_id", zethamem::ValueType::Int},
          {"name", zethamem::ValueType::String},
          {"priority", zethamem::ValueType::Int},
          {"timeout_ms", zethamem::ValueType::Int},
      };
      schema.rebuild_index();
      metadata_ = zethamem::Table(std::move(schema));
    }

    const std::int64_t timeout = t.metadata().timeout ? t.metadata().timeout->count() : -1;
    metadata_.insert({
        {"task_id", static_cast<std::int64_t>(t.id())},
        {"name", t.metadata().name},
        {"priority", static_cast<std::int64_t>(static_cast<int>(t.metadata().prio))},
        {"timeout_ms", timeout},
    });
  }

  graph_t graph_;
  std::unordered_map<task_id, std::shared_ptr<task_base>> tasks_;
  zethamem::Table metadata_;
};

class scheduler {
public:
  explicit scheduler(std::size_t workers = std::max<std::size_t>(1, std::thread::hardware_concurrency()))
      : workers_(workers), deques_(workers) {}

  void push_ready(const std::shared_ptr<task_base>& t) {
    const auto idx = t->id() % workers_;
    std::lock_guard<std::mutex> lock(deques_[idx].m);
    deques_[idx].q.push_back(t);
  }

  std::shared_ptr<task_base> pop_local(std::size_t i) {
    std::lock_guard<std::mutex> lock(deques_[i].m);
    if (deques_[i].q.empty()) return {};
    auto t = deques_[i].q.back();
    deques_[i].q.pop_back();
    return t;
  }

  std::shared_ptr<task_base> steal_fifo(std::size_t thief) {
    for (std::size_t i = 0; i < workers_; ++i) {
      if (i == thief) continue;
      std::lock_guard<std::mutex> lock(deques_[i].m);
      if (!deques_[i].q.empty()) {
        auto t = deques_[i].q.front();
        deques_[i].q.pop_front();
        // PERF(codex): replace linear victim scan with randomized/telemetry-guided stealing.
        return t;
      }
    }
    return {};
  }

  std::size_t workers() const noexcept { return workers_; }

private:
  struct worker_deque { std::mutex m; std::deque<std::shared_ptr<task_base>> q; };
  std::size_t workers_;
  std::vector<worker_deque> deques_;
};

class execution_context {
public:
  explicit execution_context(task_graph g,
                             std::size_t workers = std::max<std::size_t>(1, std::thread::hardware_concurrency()))
      : graph_(std::move(g)), sched_(workers) {}

  void run_all() {
    const auto order = graph_.schedule();
    auto& tasks = graph_.tasks();

    std::unordered_map<task_id, std::atomic<std::size_t>> pending;
    std::unordered_map<task_id, std::vector<task_id>> dependents;

    for (auto id : order) {
      auto it = tasks.find(id);
      if (it == tasks.end()) continue;

      pending[id].store(it->second->dependencies().size(), std::memory_order_relaxed);
      if (pending[id].load(std::memory_order_relaxed) == 0) {
        it->second->set_state(task_state::ready);
        sched_.push_ready(it->second);
      }

      for (auto dep : it->second->dependencies()) {
        dependents[dep].push_back(id);
      }
    }

    std::atomic<std::size_t> remaining{tasks.size()};
    std::mutex done_m;
    std::condition_variable done_cv;
    std::vector<std::thread> pool;

    for (std::size_t i = 0; i < sched_.workers(); ++i) {
      pool.emplace_back([&, i] {
        while (remaining.load(std::memory_order_acquire) > 0) {
          auto task = sched_.pop_local(i);
          if (!task) task = sched_.steal_fifo(i);
          if (!task) {
            std::this_thread::yield();
            continue;
          }

          task->run();
          const auto sid = task->id();

          if (task->state() == task_state::failed || task->state() == task_state::cancelled) {
            for (auto dep : dependents[sid]) {
              tasks.at(dep)->cancel();
              if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lk(done_m);
                done_cv.notify_all();
              }
            }
          } else {
            for (auto dep : dependents[sid]) {
              if (pending[dep].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                tasks.at(dep)->set_state(task_state::ready);
                sched_.push_ready(tasks.at(dep));
              }
            }
          }

          if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lk(done_m);
            done_cv.notify_all();
          }
        }
      });
    }

    std::unique_lock<std::mutex> lk(done_m);
    done_cv.wait(lk, [&] { return remaining.load(std::memory_order_acquire) == 0; });
    lk.unlock();

    for (auto& t : pool) {
      if (t.joinable()) t.join();
    }
  }

  zethamem::Value wait(task_id id) const {
    auto it = graph_.tasks().find(id);
    if (it == graph_.tasks().end()) throw task_error("unknown task id");
    return it->second->wait();
  }

private:
  task_graph graph_;
  scheduler sched_;
};

} // namespace polyflow

#endif
