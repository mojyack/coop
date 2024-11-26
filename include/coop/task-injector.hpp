#pragma once
#include "generator.hpp"
#include "promise.hpp"
#include "recursive-blocker.hpp"
#include "runner.hpp"
#include "task-injector-pre.hpp"

namespace coop {
template <class T>
inline auto TaskInjector::inject_task(coop::Async<T> task) -> T {
    constexpr auto is_ret_void = std::is_same_v<T, void>;
    using RetStorage           = std::conditional_t<is_ret_void, bool, T>;

    auto ret  = RetStorage();
    auto done = coop::AtomicEvent();

    blocker.block();
    runner->push_task([](coop::Async<T> task, RetStorage& ret, coop::AtomicEvent& done) -> coop::Async<void> {
        if constexpr(is_ret_void) {
            co_await task;
        } else {
            ret = co_await task;
        }
        done.notify();
    }(std::move(task), ret, done));
    blocker.unblock();

    done.wait();
    if constexpr(is_ret_void) {
        return;
    } else {
        return ret;
    }
}

inline TaskInjector::TaskInjector(coop::Runner& runner)
    : runner(&runner) {
    blocker.start(runner);
}
} // namespace coop
