#pragma once
#include <chrono>

#include "cohandle.hpp"

namespace coop {
struct [[nodiscard]] TimeAwaiter {
    std::chrono::system_clock::duration duration;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) const -> void;
    auto await_resume() const -> void;

    TimeAwaiter(std::chrono::system_clock::duration duration);
};

using sleep = TimeAwaiter;
} // namespace coop
