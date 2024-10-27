#pragma once
#include <coroutine>
#include <vector>

namespace coop {
struct Task;
struct Runner;

template <class T>
struct Promise;

struct [[nodiscard]] Event {
    Runner*            runner;
    std::vector<Task*> waiters;

    auto await_ready() const -> bool { return false; }
    auto await_suspend(std::coroutine_handle<Promise<void>> caller_task) -> void;
    auto await_resume() const -> void {}

    auto notify() -> void;
};
} // namespace coop
