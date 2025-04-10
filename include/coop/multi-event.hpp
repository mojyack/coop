#pragma once
#include "multi-event-pre.hpp"
#include "runner.hpp"

namespace coop {
template <CoHandleLike CoHandle>
inline auto MultiEvent::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*this);
}

inline auto MultiEvent::notify(const size_t n) -> void {
    if(runner != nullptr) {
        runner->event_notify(*this, n);
    }
}
} // namespace coop
