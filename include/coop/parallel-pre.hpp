#pragma once
#include <tuple>
#include <vector>

#include "generator-pre.hpp"

namespace coop {
template <CoGeneratorLike... Generators>
struct [[nodiscard]] ParallelArgsAwaiter /* ParallelAwaiter */ {
    std::tuple<Generators...> generators;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    ParallelArgsAwaiter(Generators&&... generators);
};

template <CoGeneratorLike... Generators>
using run_args = ParallelArgsAwaiter<Generators...>;

template <CoGeneratorLike Generator>
struct [[nodiscard]] ParallelVecAwaiter {
    std::vector<Generator> generators;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    ParallelVecAwaiter(std::vector<Generator> generators);
};

template <CoGeneratorLike Generator>
using run_vec = ParallelVecAwaiter<Generator>;
} // namespace coop
