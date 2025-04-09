#pragma once
#include "generator.hpp"
#include "multi-event-pre.hpp"

namespace coop {
struct Mutex {
    MultiEvent event;
    bool       held = false;

    auto lock() -> coop::Async<void>;
    auto unlock() -> void;
};
} // namespace coop

