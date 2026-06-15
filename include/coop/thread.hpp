#pragma once
#include <functional>

#include "thread-event.hpp"
#include "thread-pre.hpp"

namespace coop {
template <class Functor>
auto ThreadRunAdapter<Functor>::await_ready() const -> bool {
    return false;
}

template <class Functor>
template <CoHandleLike CoHandle>
auto ThreadRunAdapter<Functor>::await_suspend(CoHandle caller_task) -> void {
    thread->task = [this]() {
        if constexpr(!void_ret) {
            ret = functor();
        } else {
            functor();
        }
    };
    thread->wakeup.test_and_set();
    thread->wakeup.notify_one();
    thread->event.await_suspend(caller_task);
}

template <class Functor>
auto ThreadRunAdapter<Functor>::await_resume() -> Ret {
    thread->event.await_resume();
    if constexpr(!void_ret) {
        return std::move(ret);
    }
}

template <class Functor>
auto Thread::run(Functor functor) -> ThreadRunAdapter<Functor> {
    return ThreadRunAdapter<Functor>{
        .thread  = this,
        .functor = std::move(functor),
    };
}

inline auto Thread::loop() -> void {
loop:
    wakeup.wait(false);
    wakeup.clear();
    if(stopping.test()) {
        return;
    }
    const auto current = std::move(task);
    current();
    event.notify();
    goto loop;
}

inline Thread::Thread(std::function<void()> init) {
    thread = std::thread([this, init = std::move(init)]() {
        if(init) {
            init();
        }
        loop();
    });
}

inline Thread::~Thread() {
    stopping.test_and_set();
    wakeup.test_and_set();
    wakeup.notify_one();
    if(thread.joinable()) {
        thread.join();
    }
}
} // namespace coop
