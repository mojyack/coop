#pragma once
#include "cohandle.hpp"

namespace coop {
struct Runner;
struct Task;
struct TaskHandle;

struct [[nodiscard]] TaskJoiner {
    TaskHandle* handle;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void;

    TaskJoiner(TaskHandle& handle);
};

struct TaskHandle {
    Task*   task = nullptr;
    Runner* runner;
    bool    destroyed;

    auto cancel() -> bool;
    auto dissociate() -> void;
    auto join() -> TaskJoiner;
};
} // namespace coop
