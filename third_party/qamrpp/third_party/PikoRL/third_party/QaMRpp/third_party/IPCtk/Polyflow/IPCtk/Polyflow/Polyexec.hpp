#pragma once
/*
 * Polyexec.hpp
 *
 * Execution layer for Polyflow: task scheduling, process orchestration,
 * and runtime constraint enforcement.
 *
 * Integrates:
 *   - ExecShims.hpp: process execution primitives
 *   - Grafitt.hpp: task graph representation
 *   - ZethaMEM.hpp: typed value model for task I/O
 *   - DSLUtils.hpp: fluent DSL composition
 *
 * Design principles:
 *   - Header-only
 *   - Zero-cost abstractions where possible
 *   - Explicit error propagation via exceptions
 *   - C++20 concepts for type safety
 */

#include "PolyflowData.hpp"
#include "PolyflowDSLUtils.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>
#include <future>
#include <cmath>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>

namespace polyexec {

// ============================================================================
// Error Hierarchy (extends execshims::ExecError)
// ============================================================================

class constraint_violation : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class deadlock_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class timeout_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class task_not_found : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ============================================================================
// Task State Machine
// ============================================================================

enum class TaskState : std::uint8_t {
    Pending,
    Ready,
    Running,
    Blocked,
    Completed,
    Failed,
    Cancelled
};

inline std::string to_string(TaskState s) {
    switch (s) {
        case TaskState::Pending: return "Pending";
        case TaskState::Ready: return "Ready";
        case TaskState::Running: return "Running";
        case TaskState::Blocked: return "Blocked";
        case TaskState::Completed: return "Completed";
        case TaskState::Failed: return "Failed";
        case TaskState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

// ============================================================================
// Task Priority
// ============================================================================

enum class Priority : std::uint8_t {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Background = 4
};

// ============================================================================
// Task I/O Model (using zethamem::Value)
// ============================================================================

using TaskInput = std::unordered_map<std::string, zethamem::Value>;
using TaskOutput = std::unordered_map<std::string, zethamem::Value>;

struct TaskResult {
    TaskState state = TaskState::Pending;
    TaskOutput output;
    std::optional<std::string> error_msg;
    int exit_code = 0;
    
    bool is_success() const noexcept {
        return state == TaskState::Completed && exit_code == 0;
    }
};

// ============================================================================
// Task Handle (opaque ID)
// ============================================================================

struct TaskHandle {
    std::uint64_t id = 0;
    
    bool operator==(const TaskHandle& other) const noexcept {
        return id == other.id;
    }
    
    explicit operator bool() const noexcept { return id != 0; }
};

} // namespace polyexec

// Hash specialization for TaskHandle
template<>
struct std::hash<polyexec::TaskHandle> {
    std::size_t operator()(const polyexec::TaskHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id);
    }
};

namespace polyexec {

// ============================================================================
// Mock ExecShims API (since ExecShims library not present)
// HINT(codex): Minimal process execution wrapper for Stage 1.2 validation
// ============================================================================

struct ProcessResult {
    int exit_code = 0;
    std::string stdout_data;
    std::string stderr_data;
    
    bool is_success() const noexcept { return exit_code == 0; }
};

struct ResourceLimits {
    std::optional<size_t> max_memory_bytes;
    std::optional<std::chrono::milliseconds> timeout;
    std::optional<int> cpu_affinity;
};

class ProcessBuilder {
    std::string cmd_;
    std::vector<std::string> args_;
    std::unordered_map<std::string, std::string> env_;
    ResourceLimits limits_;
    
public:
    ProcessBuilder(std::string cmd) : cmd_(std::move(cmd)) {}
    
    ProcessBuilder& args(std::vector<std::string> a) {
        args_ = std::move(a);
        return *this;
    }
    
    ProcessBuilder& env(std::unordered_map<std::string, std::string> e) {
        env_ = std::move(e);
        return *this;
    }
    
    ProcessBuilder& limits(ResourceLimits l) {
        limits_ = std::move(l);
        return *this;
    }
    
    ProcessResult run() {
        ProcessResult result;
        
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            result.exit_code = -1;
            result.stderr_data = "pipe() failed";
            return result;
        }
        
        pid_t pid = fork();
        if (pid == -1) {
            close(pipefd[0]);
            close(pipefd[1]);
            result.exit_code = -1;
            result.stderr_data = "fork() failed";
            return result;
        }
        
        if (pid == 0) {
            // Child
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            for (const auto& [key, value] : env_) {
                ::setenv(key.c_str(), value.c_str(), 1);
            }

            if (limits_.max_memory_bytes) {
                rlimit limit{};
                limit.rlim_cur = *limits_.max_memory_bytes;
                limit.rlim_max = *limits_.max_memory_bytes;
                ::setrlimit(RLIMIT_AS, &limit);
                ::setrlimit(RLIMIT_DATA, &limit);
            }

#if defined(__linux__)
            if (limits_.cpu_affinity) {
                cpu_set_t set;
                CPU_ZERO(&set);
                CPU_SET(*limits_.cpu_affinity, &set);
                ::sched_setaffinity(0, sizeof(set), &set);
            }
#endif
            
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(cmd_.c_str()));
            for (auto& a : args_) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            
            execvp(cmd_.c_str(), argv.data());
            _exit(127);
        }
        
        // Parent
        close(pipefd[1]);
        int flags = fcntl(pipefd[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
        }
        
        char buf[4096];
        int status = 0;
        const auto start = std::chrono::steady_clock::now();
        bool child_exited = false;
        while (!child_exited) {
            for (;;) {
                const ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
                if (n > 0) {
                    result.stdout_data.append(buf, static_cast<std::size_t>(n));
                    continue;
                }
                if (n == 0) break;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                break;
            }

            const pid_t wait_result = ::waitpid(pid, &status, WNOHANG);
            if (wait_result == pid) {
                child_exited = true;
                break;
            }
            if (wait_result == -1) {
                result.exit_code = -1;
                result.stderr_data = "waitpid() failed";
                break;
            }

            if (limits_.timeout && (std::chrono::steady_clock::now() - start) > *limits_.timeout) {
                ::kill(pid, SIGKILL);
                ::waitpid(pid, &status, 0);
                result.exit_code = -1;
                result.stderr_data = "timeout exceeded";
                child_exited = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        for (;;) {
            const ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
            if (n > 0) {
                result.stdout_data.append(buf, static_cast<std::size_t>(n));
                continue;
            }
            break;
        }
        close(pipefd[0]);

        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else {
            result.exit_code = (result.exit_code == -1) ? -1 : result.exit_code;
        }
        
        return result;
    }
};

inline ProcessResult run_command_checked(const std::string& cmd, 
                                          const std::vector<std::string>& args = {},
                                          const std::unordered_map<std::string, std::string>& env = {}) {
    return ProcessBuilder(cmd).args(args).env(env).run();
}

// ============================================================================
// Retry Policy
// ============================================================================

struct RetryPolicy {
    int max_attempts = 1;
    std::chrono::milliseconds initial_delay{100};
    double backoff_multiplier = 2.0;
    std::chrono::milliseconds max_delay{30000};
    
    std::chrono::milliseconds delay_for_attempt(int attempt) const {
        if (attempt <= 0) return std::chrono::milliseconds{0};
        auto delay = initial_delay * static_cast<int>(std::pow(backoff_multiplier, attempt - 1));
        return std::min(delay, max_delay);
    }
};

// ============================================================================
// Task Base (abstract)
// ============================================================================

class TaskBase {
protected:
    TaskHandle handle_;
    TaskState state_{TaskState::Pending};
    Priority priority_{Priority::Normal};
    std::optional<std::chrono::milliseconds> timeout_;
    RetryPolicy retry_policy_;
    int attempt_count_{0};
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    TaskResult result_;
    std::exception_ptr exception_;
    
public:
    TaskBase() {
        static std::atomic<std::uint64_t> next_id{1};
        handle_.id = next_id.fetch_add(1, std::memory_order_relaxed);
    }
    
    virtual ~TaskBase() = default;
    
    TaskHandle handle() const noexcept { return handle_; }
    
    TaskState state() const {
        std::lock_guard lock(mutex_);
        return state_;
    }
    
    void set_priority(Priority p) {
        std::lock_guard lock(mutex_);
        priority_ = p;
    }
    
    Priority priority() const {
        std::lock_guard lock(mutex_);
        return priority_;
    }
    
    void set_timeout(std::chrono::milliseconds t) {
        std::lock_guard lock(mutex_);
        timeout_ = t;
    }
    
    void set_retry_policy(RetryPolicy p) {
        std::lock_guard lock(mutex_);
        retry_policy_ = std::move(p);
    }
    
    virtual void execute(const TaskInput& input) = 0;
    
    void cancel() {
        std::lock_guard lock(mutex_);
        if (state_ == TaskState::Pending || state_ == TaskState::Ready) {
            state_ = TaskState::Cancelled;
            result_.state = TaskState::Cancelled;
            cv_.notify_all();
        }
    }
    
    TaskResult wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] {
            return state_ == TaskState::Completed || 
                   state_ == TaskState::Failed || 
                   state_ == TaskState::Cancelled;
        });
        
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        
        return result_;
    }
    
    std::optional<TaskResult> try_get() {
        std::lock_guard lock(mutex_);
        if (state_ == TaskState::Completed || 
            state_ == TaskState::Failed || 
            state_ == TaskState::Cancelled) {
            return result_;
        }
        return std::nullopt;
    }
    
protected:
    void set_state(TaskState s) {
        std::lock_guard lock(mutex_);
        state_ = s;
        result_.state = s;
        cv_.notify_all();
    }
    
    void set_result(TaskResult r) {
        std::lock_guard lock(mutex_);
        result_ = std::move(r);
        state_ = result_.state;
        cv_.notify_all();
    }
    
    void set_exception(std::exception_ptr e) {
        std::lock_guard lock(mutex_);
        exception_ = e;
        state_ = TaskState::Failed;
        result_.state = TaskState::Failed;
        cv_.notify_all();
    }
};

// ============================================================================
// Process Task
// ============================================================================

class ProcessTask : public TaskBase {
    std::string cmd_;
    std::vector<std::string> args_;
    std::unordered_map<std::string, std::string> env_;
    ResourceLimits limits_;
    
public:
    ProcessTask(std::string cmd, 
                std::vector<std::string> args = {},
                std::unordered_map<std::string, std::string> env = {})
        : cmd_(std::move(cmd)), args_(std::move(args)), env_(std::move(env)) {}
    
    void set_limits(ResourceLimits l) {
        limits_ = std::move(l);
    }
    
    void execute(const TaskInput& input) override {
        set_state(TaskState::Running);
        
        int attempt = 0;
        ProcessResult proc_result;
        
        while (attempt < retry_policy_.max_attempts) {
            attempt_count_ = ++attempt;
            
            try {
                proc_result = ProcessBuilder(cmd_)
                    .args(args_)
                    .env(env_)
                    .limits(limits_)
                    .run();
                
                if (proc_result.is_success()) {
                    break;
                }
                
                if (attempt < retry_policy_.max_attempts) {
                    auto delay = retry_policy_.delay_for_attempt(attempt);
                    std::this_thread::sleep_for(delay);
                }
            } catch (...) {
                if (attempt >= retry_policy_.max_attempts) {
                    set_exception(std::current_exception());
                    return;
                }
                auto delay = retry_policy_.delay_for_attempt(attempt);
                std::this_thread::sleep_for(delay);
            }
        }
        
        TaskResult result;
        result.exit_code = proc_result.exit_code;
        result.output["stdout"] = zethamem::Value(proc_result.stdout_data);
        result.output["stderr"] = zethamem::Value(proc_result.stderr_data);
        
        if (proc_result.is_success()) {
            result.state = TaskState::Completed;
        } else {
            result.state = TaskState::Failed;
            result.error_msg = "Process exited with code " + std::to_string(proc_result.exit_code);
        }
        
        set_result(result);
    }
};

// ============================================================================
// Task Factory
// ============================================================================

inline std::shared_ptr<ProcessTask> make_process_task(
    const std::string& cmd,
    const std::vector<std::string>& args = {},
    const std::unordered_map<std::string, std::string>& env = {}) {
    return std::make_shared<ProcessTask>(cmd, args, env);
}

// ============================================================================
// Typed Task Wrapper (Task<T>)
// ============================================================================

template<typename T>
class Task {
public:
    std::shared_ptr<TaskBase> base_;
    std::function<T(const TaskResult&)> extractor_;
    
    Task(std::shared_ptr<TaskBase> base, std::function<T(const TaskResult&)> extractor)
        : base_(std::move(base)), extractor_(std::move(extractor)) {}
    
    TaskHandle handle() const { return base_->handle(); }
    TaskState state() const { return base_->state(); }
    
    void cancel() { base_->cancel(); }
    
    T get() {
        auto result = base_->wait();
        if (!result.is_success()) {
            throw std::runtime_error(result.error_msg.value_or("Task failed"));
        }
        return extractor_(result);
    }
    
    std::optional<T> try_get() {
        auto result = base_->try_get();
        if (!result || !result->is_success()) {
            return std::nullopt;
        }
        return extractor_(*result);
    }
    
    template<typename Fn>
    auto then(Fn&& fn) -> Task<std::invoke_result_t<Fn, T>> {
        using U = std::invoke_result_t<Fn, T>;
        auto extractor = [this, fn = std::forward<Fn>(fn)](const TaskResult& r) -> U {
            T value = extractor_(r);
            return fn(value);
        };
        return Task<U>(base_, extractor);
    }
};

// ============================================================================
// Future/Promise (simplified)
// ============================================================================

template<typename T>
class Promise;

template<typename T>
class Future {
    std::shared_ptr<std::promise<T>> promise_;
    std::shared_future<T> future_;
    
    friend class Promise<T>;
    
    Future(std::shared_ptr<std::promise<T>> p) 
        : promise_(p), future_(p->get_future().share()) {}
    
public:
    T get() { return future_.get(); }
    
    bool is_ready() const {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
    template<typename U>
    Future<U> then(std::function<U(T)> fn) {
        auto new_promise = std::make_shared<std::promise<U>>();
        auto new_future = Future<U>(new_promise);
        
        std::thread([f = future_, p = new_promise, fn = std::move(fn)]() mutable {
            try {
                U result = fn(f.get());
                p->set_value(std::move(result));
            } catch (...) {
                p->set_exception(std::current_exception());
            }
        }).detach();
        
        return new_future;
    }
};

template<typename T>
class Promise {
    std::shared_ptr<std::promise<T>> promise_;
    
public:
    Promise() : promise_(std::make_shared<std::promise<T>>()) {}
    
    Future<T> get_future() {
        return Future<T>(promise_);
    }
    
    void set_value(T value) {
        promise_->set_value(std::move(value));
    }
    
    void set_exception(std::exception_ptr e) {
        promise_->set_exception(e);
    }
};

// ============================================================================
// Convenience: ProcessTask -> Task<ProcessResult>
// ============================================================================

inline Task<ProcessResult> make_typed_process_task(
    const std::string& cmd,
    const std::vector<std::string>& args = {},
    const std::unordered_map<std::string, std::string>& env = {}) {
    
    auto base = make_process_task(cmd, args, env);
    
    auto extractor = [](const TaskResult& r) -> ProcessResult {
        ProcessResult pr;
        pr.exit_code = r.exit_code;
        
        auto it_out = r.output.find("stdout");
        if (it_out != r.output.end() && std::holds_alternative<std::string>(it_out->second)) {
            pr.stdout_data = std::get<std::string>(it_out->second);
        }
        
        auto it_err = r.output.find("stderr");
        if (it_err != r.output.end() && std::holds_alternative<std::string>(it_err->second)) {
            pr.stderr_data = std::get<std::string>(it_err->second);
        }
        
        return pr;
    };
    
    return Task<ProcessResult>(base, extractor);
}

} // namespace polyexec
