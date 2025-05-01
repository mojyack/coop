#pragma once
#include <array>

#include "generator.hpp"
#include "promise.hpp"
#include "runner.hpp"
#include "single-event.hpp"
#include "task-handle.hpp"

namespace coop {
template <class... T>
    requires(std::is_same_v<T, coop::Async<void>> && ...)
auto select(T&&... args) -> coop::Async<size_t> {
    auto tasks         = std::array{std::forward<coop::Async<void>&&>(args)...};
    auto handles       = std::array<coop::TaskHandle, tasks.size()>();
    auto done          = coop::SingleEvent();
    auto complete_task = 0uz;
    auto task_template = [&](const size_t index) -> coop::Async<void> {
        co_await tasks[index];
        complete_task = index;
        done.notify();
    };
    auto& runner = *co_await coop::reveal_runner();
    for(auto i = 0uz; i < tasks.size(); i += 1) {
        runner.push_task(task_template(i), &handles[i]);
    }
    co_await done;
    for(auto& handle : handles) {
        handle.cancel();
    }
    co_return complete_task;
}
} // namespace coop
