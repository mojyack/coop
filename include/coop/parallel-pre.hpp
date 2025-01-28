#pragma once
#include <tuple>
#include <vector>

#include "generator-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
template <CoGeneratorLike... Generators>
struct [[nodiscard]] ParallelArgsAwaiter /* ParallelAwaiter */ {
    std::tuple<Generators...>                      generators;
    std::array<TaskHandle*, sizeof...(Generators)> user_handles = {};
    bool                                           independent  = false;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    [[nodiscard]] auto detach(std::array<TaskHandle*, sizeof...(Generators)> handles = {}) -> ParallelArgsAwaiter&&;

    ParallelArgsAwaiter(Generators&&... generators);
};

template <CoGeneratorLike... Generators>
using run_args = ParallelArgsAwaiter<Generators...>;

template <CoGeneratorLike Generator>
struct [[nodiscard]] ParallelVecAwaiter {
    std::vector<Generator>   generators;
    std::vector<TaskHandle*> user_handles = {};
    bool                     independent  = false;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    [[nodiscard]] auto detach(std::vector<TaskHandle*> handles) -> ParallelVecAwaiter&&;

    ParallelVecAwaiter(std::vector<Generator> generators);
};

template <CoGeneratorLike Generator>
using run_vec = ParallelVecAwaiter<Generator>;
} // namespace coop
