#pragma once
#include "timer-pre.hpp"

namespace coop {
inline auto TimeAwaiter::await_ready() const -> bool {
    return false;
}

template <CoHandleLike CoHandle>
inline auto TimeAwaiter::await_suspend(CoHandle caller_task) const -> void {
    auto& runner = *caller_task.promise().runner;
    runner.delay(duration);
}

inline auto TimeAwaiter::await_resume() const -> void {
}

inline TimeAwaiter::TimeAwaiter(const std::chrono::system_clock::duration duration)
    : duration(duration) {
}
} // namespace coop
