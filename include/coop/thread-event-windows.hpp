#pragma once
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
    runner->io_wait(pipe.producer(), true, false, result);
}

inline auto ThreadEvent::await_resume() -> void {
    const auto sock = pipe.producer();

    auto mode = u_long();

    // set to nonblocking
    mode = 1;
    coop::assert(ioctlsocket(sock, FIONBIO, &mode) == 0, "errno=", WSAGetLastError());

    [[maybe_unused]] auto count = 0;
    while(true) {
        auto buffer = std::byte();
        if(pipe.read(&buffer, 1) != 1) {
            const auto error = WSAGetLastError();
            coop::assert(error == WSAEWOULDBLOCK, "errno=", error);
            break;
        }
        count += 1;
    }

    // set to blocking
    mode = 0;
    coop::assert(ioctlsocket(sock, FIONBIO, &mode) == 0, "errno=", WSAGetLastError());
}

inline auto ThreadEvent::notify() -> void {
    coop::assert(pipe.write("", 1) == 1, "errno=", WSAGetLastError());
}

inline ThreadEvent::ThreadEvent(ThreadEvent&& o)
    : pipe(std::move(o.pipe)) {
}

inline ThreadEvent::ThreadEvent() {
}

inline ThreadEvent::~ThreadEvent() {
}
} // namespace coop
