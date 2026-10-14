/*
 * PolyflowExternalMonitor.cpp
 *
 * Standalone TUI monitor for live Polyflow taskflows.
 * Attaches via IPC to running Polyflow instances and displays real-time
 * task state, scheduler activity, and runtime metrics.
 *
 * Usage:
 *   polyflow-monitor --id <taskflow_id>
 *   polyflow-monitor --pid <process_pid>
 *   polyflow-monitor --discover
 */

#include "IPCtk/IPCtk.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

// Forward declare Polyflow types to avoid DSL collision
namespace polyflow {
    enum class TaskState : std::uint8_t {
        TS_Pending,
        TS_Ready,
        TS_Queued,
        TS_Running,
        TS_Completed,
        TS_Failed,
        TS_Cancelled
    };
    
    inline std::string to_string(TaskState s) {
        switch (s) {
            case TaskState::TS_Pending: return "Pending";
            case TaskState::TS_Ready: return "Ready";
            case TaskState::TS_Queued: return "Queued";
            case TaskState::TS_Running: return "Running";
            case TaskState::TS_Completed: return "Completed";
            case TaskState::TS_Failed: return "Failed";
            case TaskState::TS_Cancelled: return "Cancelled";
        }
        return "Unknown";
    }
}

#include <map>
#include <fstream>
#include <chrono>
#include <thread>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

namespace monitor {

// ============================================================================
// Terminal Control
// ============================================================================

struct TermSize {
    int rows = 24;
    int cols = 80;
};

inline TermSize get_term_size() {
    TermSize sz;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        sz.rows = w.ws_row;
        sz.cols = w.ws_col;
    }
    return sz;
}

inline void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

inline void move_cursor(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H";
}

inline void hide_cursor() {
    std::cout << "\033[?25l" << std::flush;
}

inline void show_cursor() {
    std::cout << "\033[?25h" << std::flush;
}

// ANSI colors
inline std::string color_gray()   { return "\033[90m"; }
inline std::string color_yellow() { return "\033[93m"; }
inline std::string color_cyan()   { return "\033[96m"; }
inline std::string color_green()  { return "\033[92m"; }
inline std::string color_blue()   { return "\033[94m"; }
inline std::string color_red()    { return "\033[91m"; }
inline std::string color_reset()  { return "\033[0m"; }

inline std::string color_for_state(polyflow::TaskState s) {
    using polyflow::TaskState;
    switch (s) {
        case TaskState::TS_Pending: return color_gray();
        case TaskState::TS_Ready: return color_yellow();
        case TaskState::TS_Queued: return color_cyan();
        case TaskState::TS_Running: return color_green();
        case TaskState::TS_Completed: return color_blue();
        case TaskState::TS_Failed: return color_red();
        case TaskState::TS_Cancelled: return color_gray();
    }
    return color_reset();
}

// ============================================================================
// Monitor Event Schema
// ============================================================================

enum class EventType : uint8_t {
    TaskCreated,
    TaskStateChanged,
    TaskStarted,
    TaskCompleted,
    TaskFailed,
    WorkerSteal,
    QueueDepthUpdate
};

struct MonitorEvent {
    EventType type;
    std::string taskflow_id;
    std::string task_id;
    polyflow::TaskState task_state;
    int worker_id = -1;
    std::chrono::steady_clock::time_point timestamp;
    int64_t execution_time_ms = 0;
    int queue_depth = 0;
};

// ============================================================================
// Task State Table
// ============================================================================

struct TaskEntry {
    std::string id;
    polyflow::TaskState state = polyflow::TaskState::TS_Pending;
    int worker_id = -1;
    int64_t exec_ms = 0;
    int retries = 0;
    std::string name;
};

struct WorkerStats {
    int tasks_run = 0;
    double avg_time_ms = 0.0;
    int steals = 0;
    int queue_depth = 0;
};

// ============================================================================
// Monitor State
// ============================================================================

class MonitorState {
    std::string taskflow_id_;
    pid_t pid_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    
    std::map<std::string, TaskEntry> tasks_;
    std::map<int, WorkerStats> workers_;
    
    int total_tasks_ = 0;
    int running_ = 0;
    int ready_ = 0;
    int pending_ = 0;
    int completed_ = 0;
    int failed_ = 0;
    int queue_depth_ = 0;
    int work_steals_ = 0;
    
public:
    MonitorState(std::string tf_id, pid_t p) 
        : taskflow_id_(std::move(tf_id)), pid_(p), 
          start_time_(std::chrono::steady_clock::now()) {}
    
    void handle_event(const MonitorEvent& ev) {
        switch (ev.type) {
            case EventType::TaskCreated:
                tasks_[ev.task_id] = TaskEntry{ev.task_id, ev.task_state, -1, 0, 0, ev.task_id};
                total_tasks_++;
                break;
                
            case EventType::TaskStateChanged:
            case EventType::TaskStarted:
            case EventType::TaskCompleted:
            case EventType::TaskFailed:
                if (tasks_.count(ev.task_id)) {
                    auto& t = tasks_[ev.task_id];
                    t.state = ev.task_state;
                    t.worker_id = ev.worker_id;
                    t.exec_ms = ev.execution_time_ms;
                }
                break;
                
            case EventType::WorkerSteal:
                work_steals_++;
                if (workers_.count(ev.worker_id)) {
                    workers_[ev.worker_id].steals++;
                }
                break;
                
            case EventType::QueueDepthUpdate:
                queue_depth_ = ev.queue_depth;
                break;
        }
        
        recompute_stats();
    }
    
    void recompute_stats() {
        running_ = ready_ = pending_ = completed_ = failed_ = 0;
        for (const auto& [id, t] : tasks_) {
            using polyflow::TaskState;
            switch (t.state) {
                case TaskState::TS_Pending: pending_++; break;
                case TaskState::TS_Ready: ready_++; break;
                case TaskState::TS_Running: running_++; break;
                case TaskState::TS_Completed: completed_++; break;
                case TaskState::TS_Failed: failed_++; break;
                default: break;
            }
        }
    }
    
    std::string uptime() const {
        auto now = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        int h = dur.count() / 3600;
        int m = (dur.count() % 3600) / 60;
        int s = dur.count() % 60;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << h << ":"
            << std::setw(2) << m << ":" << std::setw(2) << s;
        return oss.str();
    }
    
    void render(TermSize sz) {
        clear_screen();
        
        // Header
        std::cout << "Polyflow Monitor  |  Taskflow: " << taskflow_id_ 
                  << "  |  PID: " << pid_ << "\n";
        std::cout << "Workers: " << workers_.size() 
                  << " | Running: " << running_
                  << " | Ready: " << ready_
                  << " | Pending: " << pending_
                  << " | Completed: " << completed_
                  << " | Failed: " << failed_ << "\n\n";
        
        std::cout << "Queue Depth: " << queue_depth_
                  << " | Work Steals: " << work_steals_
                  << " | Uptime: " << uptime() << "\n\n";
        
        // Task table header
        std::cout << std::left
                  << std::setw(12) << "ID"
                  << std::setw(12) << "STATE"
                  << std::setw(8) << "WORKER"
                  << std::setw(10) << "EXEC(ms)"
                  << std::setw(8) << "RETRIES"
                  << "NAME\n";
        std::cout << std::string(sz.cols, '-') << "\n";
        
        // Task rows (limit to screen height)
        int row = 0;
        int max_rows = sz.rows - 10;
        for (const auto& [id, t] : tasks_) {
            if (row++ >= max_rows) break;
            
            std::string state_str = polyflow::to_string(t.state);
            std::string worker_str = (t.worker_id >= 0) ? ("W" + std::to_string(t.worker_id)) : "-";
            std::string exec_str = (t.exec_ms > 0) ? std::to_string(t.exec_ms) : "-";
            
            std::cout << color_for_state(t.state)
                      << std::left
                      << std::setw(12) << id.substr(0, 11)
                      << std::setw(12) << state_str
                      << std::setw(8) << worker_str
                      << std::setw(10) << exec_str
                      << std::setw(8) << t.retries
                      << t.name
                      << color_reset() << "\n";
        }
        
        std::cout << "\n[q] quit  [r] refresh  [w] workers  [h] help\n";
        std::cout << std::flush;
    }
    
    void render_workers(TermSize sz) {
        clear_screen();
        
        std::cout << "Worker Statistics\n\n";
        std::cout << std::left
                  << std::setw(8) << "Worker"
                  << std::setw(12) << "TasksRun"
                  << std::setw(14) << "AvgTime(ms)"
                  << std::setw(8) << "Steals"
                  << "QueueDepth\n";
        std::cout << std::string(sz.cols, '-') << "\n";
        
        for (const auto& [wid, ws] : workers_) {
            std::cout << std::left
                      << std::setw(8) << ("W" + std::to_string(wid))
                      << std::setw(12) << ws.tasks_run
                      << std::setw(14) << std::fixed << std::setprecision(1) << ws.avg_time_ms
                      << std::setw(8) << ws.steals
                      << ws.queue_depth << "\n";
        }
        
        std::cout << "\n[q] quit  [r] refresh  [ESC] back\n";
        std::cout << std::flush;
    }
};

// ============================================================================
// IPC Client
// ============================================================================

class MonitorClient {
    std::string endpoint_;
    int fd_ = -1;
    std::string buffer_;
    
public:
    MonitorClient(const std::string& taskflow_id) {
        endpoint_ = "/tmp/polyflow/" + taskflow_id + ".ipc";
    }
    
    bool connect() {
        if (fd_ >= 0) return true;
        fd_ = ::open(endpoint_.c_str(), O_RDONLY | O_NONBLOCK);
        return fd_ >= 0;
    }

    ~MonitorClient() {
        if (fd_ >= 0) ::close(fd_);
    }
    
    std::optional<MonitorEvent> poll() {
        if (fd_ < 0 && !connect()) return std::nullopt;

        std::array<char, 2048> chunk{};
        for (;;) {
            const auto n = ::read(fd_, chunk.data(), chunk.size());
            if (n > 0) {
                buffer_.append(chunk.data(), static_cast<std::size_t>(n));
                continue;
            }
            if (n == 0) {
                ::close(fd_);
                fd_ = -1;
                return std::nullopt;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            ::close(fd_);
            fd_ = -1;
            return std::nullopt;
        }

        auto line_end = buffer_.find('\n');
        if (line_end == std::string::npos) return std::nullopt;

        const std::string line = buffer_.substr(0, line_end);
        buffer_.erase(0, line_end + 1);

        std::istringstream iss(line);
        std::string type;
        MonitorEvent ev{};
        long long state = 0;
        long long timestamp_us = 0;

        if (!(iss >> type >> ev.taskflow_id >> ev.task_id >> state >> ev.worker_id >> ev.execution_time_ms >> ev.queue_depth >> timestamp_us)) {
            return std::nullopt;
        }

        if (type == "TaskCreated") ev.type = EventType::TaskCreated;
        else if (type == "TaskStateChanged") ev.type = EventType::TaskStateChanged;
        else if (type == "TaskStarted") ev.type = EventType::TaskStarted;
        else if (type == "TaskCompleted") ev.type = EventType::TaskCompleted;
        else if (type == "TaskFailed") ev.type = EventType::TaskFailed;
        else if (type == "WorkerSteal") ev.type = EventType::WorkerSteal;
        else if (type == "QueueDepthUpdate") ev.type = EventType::QueueDepthUpdate;
        else return std::nullopt;

        switch (state) {
            case 0: ev.task_state = polyflow::TaskState::TS_Pending; break;
            case 1: ev.task_state = polyflow::TaskState::TS_Ready; break;
            case 2: ev.task_state = polyflow::TaskState::TS_Queued; break;
            case 3: ev.task_state = polyflow::TaskState::TS_Running; break;
            case 4: ev.task_state = polyflow::TaskState::TS_Completed; break;
            case 5: ev.task_state = polyflow::TaskState::TS_Failed; break;
            default: ev.task_state = polyflow::TaskState::TS_Cancelled; break;
        }
        ev.timestamp = std::chrono::steady_clock::time_point{std::chrono::microseconds{timestamp_us}};
        return ev;
    }
};

// ============================================================================
// Discovery
// ============================================================================

struct TaskflowInfo {
    std::string id;
    pid_t pid;
    int tasks;
    int running;
    std::string uptime;
};

inline std::vector<TaskflowInfo> discover_taskflows() {
    std::vector<TaskflowInfo> result;
    
    DIR* dir = opendir("/tmp/polyflow");
    if (!dir) return result;
    
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".ipc") {
            std::string tf_id = name.substr(0, name.size() - 4);
            result.push_back({tf_id, 0, 0, 0, "00:00:00"});
        }
    }
    closedir(dir);
    
    return result;
}

// ============================================================================
// Main Loop
// ============================================================================

volatile sig_atomic_t g_running = 1;

void signal_handler(int) {
    g_running = 0;
}

enum class ViewMode {
    TaskTable,
    WorkerStats
};

void run_monitor(const std::string& taskflow_id, pid_t pid) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    MonitorClient client(taskflow_id);
    if (!client.connect()) {
        std::cerr << "Failed to connect to taskflow: " << taskflow_id << "\n";
        return;
    }
    
    MonitorState state(taskflow_id, pid);
    ViewMode view = ViewMode::TaskTable;
    
    // Non-blocking input setup
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    
    hide_cursor();
    
    while (g_running) {
        // Poll IPC events
        while (auto ev = client.poll()) {
            state.handle_event(*ev);
        }
        
        // Render
        auto sz = get_term_size();
        if (view == ViewMode::TaskTable) {
            state.render(sz);
        } else {
            state.render_workers(sz);
        }
        
        // Check for input (non-blocking)
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 100000}; // 100ms
        
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                switch (c) {
                    case 'q': g_running = 0; break;
                    case 'r': break; // force refresh
                    case 'w': view = ViewMode::WorkerStats; break;
                    case 27: view = ViewMode::TaskTable; break; // ESC
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    show_cursor();
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    clear_screen();
}

} // namespace monitor

// ============================================================================
// CLI Entry
// ============================================================================

void print_usage() {
    std::cout << "Usage:\n"
              << "  polyflow-monitor --id <taskflow_id>\n"
              << "  polyflow-monitor --pid <process_pid>\n"
              << "  polyflow-monitor --discover\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "--discover") {
        auto taskflows = monitor::discover_taskflows();
        if (taskflows.empty()) {
            std::cout << "No active Polyflow taskflows found.\n";
            return 0;
        }
        
        std::cout << "Active Polyflow Taskflows\n\n";
        std::cout << std::left
                  << std::setw(15) << "ID"
                  << std::setw(8) << "PID"
                  << std::setw(8) << "Tasks"
                  << std::setw(10) << "Running"
                  << "Uptime\n";
        std::cout << std::string(60, '-') << "\n";
        
        for (const auto& tf : taskflows) {
            std::cout << std::left
                      << std::setw(15) << tf.id
                      << std::setw(8) << tf.pid
                      << std::setw(8) << tf.tasks
                      << std::setw(10) << tf.running
                      << tf.uptime << "\n";
        }
        
        return 0;
    }
    
    if (mode == "--id" && argc >= 3) {
        std::string taskflow_id = argv[2];
        monitor::run_monitor(taskflow_id, 0);
        return 0;
    }
    
    if (mode == "--pid" && argc >= 3) {
        pid_t pid = std::stoi(argv[2]);
        std::string taskflow_id = "tf_" + std::to_string(pid);
        monitor::run_monitor(taskflow_id, pid);
        return 0;
    }
    
    print_usage();
    return 1;
}
