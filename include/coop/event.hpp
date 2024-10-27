#pragma once
#include "event-pre.hpp"
#include "promise.hpp"
#include "runner-pre.hpp"

namespace coop {
inline auto Event::await_suspend(std::coroutine_handle<Promise<void>> caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*this);
}

inline auto Event::notify() -> void {
    runner->event_notify(*this);
}
} // namespace coop
