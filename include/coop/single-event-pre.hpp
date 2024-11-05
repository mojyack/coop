#pragma once
#include "cohandle.hpp"

namespace coop {
struct Task;
struct Runner;

template <class T>
struct Promise;

struct [[nodiscard]] SingleEvent {
    constexpr static auto notified = uintptr_t(-1);

    Runner* runner;
    Task*   waiter = nullptr;

    auto await_ready() -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void {}

    auto notify() -> void;
};
} // namespace coop
