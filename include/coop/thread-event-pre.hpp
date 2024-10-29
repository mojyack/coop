#pragma once
#include "cohandle.hpp"
#include "io-pre.hpp"

namespace coop {
struct ThreadEvent {
    int          fd = -1;
    IOWaitResult result;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() -> void;

    auto notify() -> void;

    ThreadEvent(ThreadEvent&& o);
    ThreadEvent();
    ~ThreadEvent();
};
} // namespace coop
