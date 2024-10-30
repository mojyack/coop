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
struct Event;

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

struct ByEvent {
    Event* event;
};

struct ByIO {
    IOWaitResult* result;
    IOHandle      file;
    bool          read;
    bool          write;
};

using SuspendReason = std::variant<Running, ByTimer, ByEvent, ByIO>;

struct Task;

struct TaskHandle {
    Task*   task = nullptr;
    Runner* runner;
    bool    destroyed;

    auto cancel() -> bool;
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
    template <CoHandleLike CoHandle, CoHandleLike... CoHandles>
    auto set_runner(CoHandle& handle, CoHandles&... handles) -> void;
    auto push_task(bool independent, std::span<TaskHandle* const> user_handles, const std::span<Task> tasks) -> void;
    template <CoHandleLike... CoHandles>
    auto push_task(bool independent, bool transfer_handle, std::span<TaskHandle* const> user_handles, CoHandles... handles) -> void;
    template <CoHandleLike CoHandle>
    auto push_task(bool independent, bool transfer_handle, std::span<TaskHandle* const> user_handles, std::span<CoHandle> handles) -> void;
    auto destroy_task(Task& task) -> bool;

    // for awaiters
    auto delay(std::chrono::system_clock::duration duration) -> void;
    auto event_wait(Event& event) -> void;
    auto event_notify(Event& event) -> void;
    auto io_wait(IOHandle fd, bool read, bool write, IOWaitResult& result) -> void;

    // public
    template <CoGeneratorLike... Generators>
    auto push_task(Generators... generators) -> void;
    template <CoGeneratorLike... Generators>
    auto push_task(std::span<TaskHandle* const> user_handles, Generators... generators) -> void;
    auto cancel_taks(TaskHandle& handle) -> bool;
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
