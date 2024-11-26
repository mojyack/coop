#pragma once
#include "generator.hpp"
#include "recursive-blocker-pre.hpp"

namespace coop {
struct TaskInjector {
    coop::Runner*    runner;
    RecursiveBlocker blocker;

    // push task from another thread and wait for completion
    template <class T>
    auto inject_task(coop::Async<T> task) -> T;

    TaskInjector(coop::Runner& runner);
};
} // namespace coop
