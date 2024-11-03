#pragma once
#include <vector>

#include "cohandle.hpp"

namespace coop {
struct Task;
struct Runner;

template <class T>
struct Promise;

struct [[nodiscard]] MultiEvent {
    Runner*            runner;
    std::vector<Task*> waiters;

    auto await_ready() const -> bool { return false; }
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void {}

    auto notify() -> void;
};
} // namespace coop
