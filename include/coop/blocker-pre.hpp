#pragma once
#include "atomic-event.hpp"
#include "coop/runner-pre.hpp"
#include "thread-pre.hpp"

namespace coop {
struct Blocker {
    coop::TaskHandle  handle;
    coop::ThreadEvent do_block_event;
    AtomicEvent       blocking_flag;
    AtomicEvent       do_unblock_event;

    auto start(coop::Runner& runner) -> void;
    auto stop() -> void;
    auto block() -> void;
    auto unblock() -> void;

    ~Blocker();
};
} // namespace coop
