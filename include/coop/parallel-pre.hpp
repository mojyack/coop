#pragma once
#include <tuple>
#include <vector>

#include "generator-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
template <CoGeneratorLike... Generators>
struct [[nodiscard]] run_args /* ParallelAwaiter */ {
    std::tuple<Generators...>                      generators;
    std::array<TaskHandle*, sizeof...(Generators)> user_handles = {};
    bool                                           independent  = false;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    [[nodiscard]] auto detach(std::array<TaskHandle*, sizeof...(Generators)> handles = {}) -> run_args&&;

    run_args(Generators&&... generators);
};

// P1814R0, merged in llvm-19
// template <CoGeneratorLike... Generators>
// using run_args = ParallelAwaiter<Generators...>;

template <CoGeneratorLike Generator>
struct [[nodiscard]] run_vec /* MonoParallelAwaiter */ {
    std::vector<Generator>   generators;
    std::vector<TaskHandle*> user_handles = {};
    bool                     independent  = false;

    auto await_ready() const -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> decltype(auto);

    [[nodiscard]] auto detach(std::vector<TaskHandle*> handles) -> run_vec&&;

    run_vec(std::vector<Generator> generators);
};

// template <CoGeneratorLike Generator>
// using run_vec = MonoParallelAwaiter<Generator>;
} // namespace coop
