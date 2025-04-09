#pragma once
#include "parallel-pre.hpp"
#include "promise-pre.hpp"
#include "runner.hpp"

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
        [&runner](auto&... generators) {
            ([&generators, &runner]() { runner.push_task(false, false, generators.handle, nullptr, 0); }(), ...);
        },
        generators);
}

template <CoGeneratorLike... Generators>
auto ParallelArgsAwaiter<Generators...>::await_resume() const -> decltype(auto) {
    constexpr auto is_all_has_value = (PromiseWithRetValue<decltype(Generators::handle.promise())> && ...);
    if constexpr(is_all_has_value) {
        return std::apply([&](auto&... generators) { return std::tuple(std::move(generators.handle.promise().data)...); }, generators);
    } else {
        return;
    }
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
    auto& runner = *caller_task.promise().runner;
    for(auto& generator : generators) {
        runner.push_task(false, false, generator.handle, nullptr, 0);
    }
}

template <CoGeneratorLike Generator>
auto ParallelVecAwaiter<Generator>::await_resume() const -> decltype(auto) {
    if constexpr(PromiseWithRetValue<decltype(Generator::handle.promise())>) {
        auto ret = std::vector<decltype(Generator::handle.promise().data)>();
        for(auto& generator : generators) {
            ret.push_back(std::move(generator.handle.promise().data));
        }
        return ret;
    } else {
        return;
    }
}

template <CoGeneratorLike Generator>
ParallelVecAwaiter<Generator>::ParallelVecAwaiter(std::vector<Generator> generators)
    : generators(std::move(generators)) {
}
} // namespace coop
