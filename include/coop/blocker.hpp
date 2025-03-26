#pragma once
#include "blocker-pre.hpp"
#include "generator.hpp"
#include "promise.hpp"
#include "thread-event.hpp"

namespace coop {
inline auto Blocker::start(coop::Runner& runner) -> void {
    runner.push_task(
        [](Blocker& self) -> coop::Async<void> {
            loop:
                co_await self.do_block_event; // wait for block()
                self.blocking_flag.notify();  // indicate runner is blocked
                self.do_unblock_event.wait(); // wait for unblock()
                goto loop;
        }(*this),
        &handle);
}

inline auto Blocker::stop() -> void {
    handle.cancel();
}

inline auto Blocker::block() -> void {
    do_block_event.notify();
    blocking_flag.wait();
}

inline auto Blocker::unblock() -> void {
    do_unblock_event.notify();
}

inline Blocker::~Blocker() {
    stop();
}
} // namespace coop
