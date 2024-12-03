#pragma once
#include <utility>

#include "runner.hpp"
#include "single-event-pre.hpp"

namespace coop {
inline auto SingleEvent::await_ready() -> bool {
    return std::exchange(waiter, nullptr) == (Task*)notified;
}

template <CoHandleLike CoHandle>
inline auto SingleEvent::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*this);
}

inline auto SingleEvent::notify() -> void {
    if(waiter != nullptr && waiter != (Task*)notified) {
        runner->event_notify(*this);
    } else {
        waiter = (Task*)notified;
    }
}
} // namespace coop
