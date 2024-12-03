#pragma once
#include <utility>

#include <sys/eventfd.h>
#include <unistd.h>

#include "runner.hpp"
#include "thread-event-pre.hpp"

namespace coop {
inline auto ThreadEvent::await_ready() const -> bool {
    return false;
}

template <CoHandleLike CoHandle>
inline auto ThreadEvent::await_suspend(CoHandle caller_task) -> void {
    const auto runner = caller_task.promise().runner;
    runner->io_wait(fd, true, false, result);
}

inline auto ThreadEvent::await_resume() -> size_t {
    auto count = eventfd_t();
    eventfd_read(fd, &count);
    return count;
}

inline auto ThreadEvent::notify() -> void {
    eventfd_write(fd, 1);
}

inline ThreadEvent::ThreadEvent(ThreadEvent&& o)
    : fd(std::exchange(o.fd, -1)) {
}

inline ThreadEvent::ThreadEvent()
    : fd(eventfd(0, 0)) {
}

inline ThreadEvent::~ThreadEvent() {
    if(fd != -1) {
        close(fd);
    }
}
} // namespace coop
