#pragma once
#include "cohandle.hpp"
#include "io-pre.hpp"

// emulate eventfd with socket pipe
#if defined(_WIN32)
#include "pipe.hpp"
#endif

namespace coop {
struct ThreadEvent {
#if defined(_WIN32)
    Pipe pipe;
#else
    int fd = -1;
#endif
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
