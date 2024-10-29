#pragma once
#include "event-pre.hpp"
#include "runner.hpp"

namespace coop {
template <CoHandleLike CoHandle>
inline auto Event::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*this);
}

inline auto Event::notify() -> void {
    runner->event_notify(*this);
}
} // namespace coop
