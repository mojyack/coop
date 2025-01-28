#pragma once
#include <span>

#include "parallel-pre.hpp"
#include "promise-pre.hpp"

namespace coop {
template <CoGeneratorLike... Generators>
auto ParallelArgsAwaiter<Generators...>::await_ready() const -> bool {
    return false;
}

template <CoGeneratorLike... Generators>
template <CoHandleLike CoHandle>
auto ParallelArgsAwaiter<Generators...>::await_suspend(CoHandle caller_task) -> void {
    auto& runner = *caller_task.promise().runner;
    std::apply(
        [this, &runner](auto&... generators) {
            // transfer cohandles to runner if detached
            // since child tasks may live longer than this generator
            const auto transfer = independent;
            runner.push_task(independent, transfer, user_handles, generators.handle...);
            if(transfer) {
                // we lost ownership
                ([&generators]() { generators.handle = nullptr; }(), ...);
            }
        },
        generators);
}

template <CoGeneratorLike... Generators>
auto ParallelArgsAwaiter<Generators...>::await_resume() const -> decltype(auto) {
    // printf("resume %p\n", std::get<0>(generators).handle.address());
    constexpr auto is_all_has_value = (PromiseWithRetValue<decltype(Generators::handle.promise())> && ...);
    if constexpr(is_all_has_value) {
        if(independent) {
            // if detached, we probably do not have return values yet!
            return std::apply([&](auto&... generators) { return std::tuple((decltype(generators.handle.promise().data)())...); }, generators);
        } else {
            return std::apply([&](auto&... generators) { return std::tuple(std::move(generators.handle.promise().data)...); }, generators);
        }
    } else {
        return;
    }
}

template <CoGeneratorLike... Generators>
auto ParallelArgsAwaiter<Generators...>::detach(std::array<TaskHandle*, sizeof...(Generators)> handles) -> ParallelArgsAwaiter&& {
    user_handles = handles;
    independent  = true;
    return std::move(*this);
}

template <CoGeneratorLike... Generators>
ParallelArgsAwaiter<Generators...>::ParallelArgsAwaiter(Generators&&... generators)
    : generators(std::forward<Generators&&>(generators)...) {
}

template <CoGeneratorLike Generator>
auto ParallelVecAwaiter<Generator>::await_ready() const -> bool {
    return false;
}

template <CoGeneratorLike Generator>
template <CoHandleLike CoHandle>
auto ParallelVecAwaiter<Generator>::await_suspend(CoHandle caller_task) -> void {
    auto& runner  = *caller_task.promise().runner;
    auto  handles = std::vector<decltype(Generator::handle)>();
    for(auto& generator : generators) {
        handles.push_back(generator.handle);
    }
    const auto transfer = independent;
    runner.push_task(independent, transfer, user_handles, std::span(handles));
    if(transfer) {
        for(auto& generator : generators) {
            generator.handle = nullptr;
        }
    }
}

template <CoGeneratorLike Generator>
auto ParallelVecAwaiter<Generator>::await_resume() const -> decltype(auto) {
    if constexpr(PromiseWithRetValue<decltype(Generator::handle.promise())>) {
        auto ret = std::vector<decltype(Generator::handle.promise().data)>();
        if(independent) {
            return ret;
        }
        for(auto& generator : generators) {
            ret.push_back(std::move(generator.handle.promise().data));
        }
        return ret;
    } else {
        return;
    }
}

template <CoGeneratorLike Generators>
auto ParallelVecAwaiter<Generators>::detach(std::vector<TaskHandle*> handles) -> ParallelVecAwaiter&& {
    user_handles = std::move(handles);
    independent  = true;
    return std::move(*this);
}

template <CoGeneratorLike Generator>
ParallelVecAwaiter<Generator>::ParallelVecAwaiter(std::vector<Generator> generators)
    : generators(std::move(generators)) {
}
} // namespace coop
