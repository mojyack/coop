#pragma once
#include <chrono>
#include <coroutine>
#include <list>
#include <span>
#include <variant>

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include "cohandle.hpp"
#include "generator-pre.hpp"

namespace coop {
struct IOWaitResult;
struct SingleEvent;
struct MultiEvent;

#if defined(_WIN32)
using IOHandle = SOCKET;
#else
using IOHandle = int;
#endif

struct Running {
};

struct ByTimer {
    std::chrono::system_clock::time_point suspend_until;
};

struct BySingleEvent {
    SingleEvent* event;
};

struct ByMultiEvent {
    MultiEvent* event;
};

struct ByIO {
    IOWaitResult* result;
    IOHandle      file;
    bool          read;
    bool          write;
};

using SuspendReason = std::variant<Running, ByTimer, BySingleEvent, ByMultiEvent, ByIO>;

struct Task;

struct TaskHandle {
    Task*   task = nullptr;
    Runner* runner;
    bool    destroyed;

    auto cancel() -> bool;
    auto dissociate() -> void;
};

struct Task {
    std::coroutine_handle<> handle;
    Task*                   parent;
    TaskHandle*             user_handle = nullptr;
    std::list<Task>         children;
    SuspendReason           suspend_reason;
    bool                    handle_owned = true;
};

struct Runner {
    // private
    Task  root;
    Task* current_task = &root;

    auto run_tasks(std::span<Task*> tasks) -> void;

    // internal
    template <CoHandleLike CoHandle>
    auto push_task(bool independent, CoHandle& handle, TaskHandle* user_handle) -> void;
    auto destroy_task(Task& task) -> bool;

    // for awaiters
    auto delay(std::chrono::system_clock::duration duration) -> void;
    auto event_wait(SingleEvent& event) -> void;
    auto event_notify(SingleEvent& event) -> void;
    auto event_wait(MultiEvent& event) -> void;
    auto event_notify(MultiEvent& event) -> void;
    auto io_wait(IOHandle fd, bool read, bool write, IOWaitResult& result) -> void;

    // public
    template <CoGeneratorLike Generator>
    auto push_task(Generator generator, TaskHandle* user_handle = nullptr) -> void;
    auto cancel_task(TaskHandle& handle) -> bool;
    auto run() -> void;
};

struct RunnerGetter {
    Runner* runner;

    auto await_ready() const -> bool {
        return false;
    }

    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void {
        runner = caller_task.promise().runner;
    }

    auto await_resume() const -> Runner* {
        return runner;
    }
};

using reveal_runner = RunnerGetter;
} // namespace coop
