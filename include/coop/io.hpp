#pragma once
#include "io-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
inline auto IOAWaiter::await_ready() const -> bool {
    return false;
}

template <CoHandleLike CoHandle>
inline auto IOAWaiter::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->io_wait(file, read, write, result);
}

inline auto IOAWaiter::await_resume() const -> IOWaitResult {
    return result;
}

inline IOAWaiter::IOAWaiter(const int file, const bool read, const bool write)
    : file(file),
      read(read),
      write(write) {}
} // namespace coop
