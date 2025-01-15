#include <algorithm>
#include <chrono>
#include <cstdlib>

// #define COOP_DEBUG
// #define COOP_TRACE

#include <coop/blocker.hpp>
#include <coop/generator.hpp>
#include <coop/io.hpp>
#include <coop/multi-event.hpp>
#include <coop/parallel.hpp>
#include <coop/pipe.hpp>
#include <coop/promise.hpp>
#include <coop/recursive-blocker.hpp>
#include <coop/runner.hpp>
#include <coop/single-event.hpp>
#include <coop/task-injector.hpp>
#include <coop/thread.hpp>
#include <coop/timer.hpp>
#include <unistd.h>

#define PRINT(...) coop::print(coop::format_filename(__FILE__), ":", __LINE__ __VA_OPT__(, " ", ) __VA_ARGS__)
#define PANIC(...)                                                                                                  \
    {                                                                                                               \
        coop::warn("panicked at ", coop::format_filename(__FILE__), ":", __LINE__ __VA_OPT__(, " ", ) __VA_ARGS__); \
        std::terminate();                                                                                           \
    }
#define ASSERT(cond, ...)                                          \
    if(!(cond)) {                                                  \
        PANIC("assertion failed" __VA_OPT__(, " ", ) __VA_ARGS__); \
    }

auto speed_rate = 1.0;

auto delay_secs(int seconds) -> coop::Async<int> {
    PRINT("delay ", seconds);
    co_await coop::sleep(std::chrono::milliseconds(size_t(1000 * seconds / speed_rate)));
    co_return seconds;
}

auto sleep_secs(int seconds) -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds(size_t(1000 * seconds / speed_rate)));
}

auto some_heavy_work() -> coop::Async<void> {
    PRINT("doing some work...");
    co_await delay_secs(3);
    co_return;
}

auto some_heavy_work_result() -> coop::Async<bool> {
    PRINT("doing some work...");
    co_await delay_secs(3);
    co_return true;
}

auto sleep_test() -> coop::Async<int> {
    {
        PRINT("count = ", co_await delay_secs(3));
        co_await some_heavy_work();
    }
    {
        auto work1 = some_heavy_work();
        auto work2 = some_heavy_work();
        auto work3 = some_heavy_work();
        /* void results = */ co_await coop::run_args(std::move(work1), std::move(work2), std::move(work3));
    }
    {
        const auto results = co_await coop::run_args(some_heavy_work_result(), some_heavy_work_result());
        const auto all_ok  = std::apply([](const auto... result) { return (result && ...); }, results);
        PRINT("result 1=", std::get<0>(results), " 2=", std::get<1>(results), " all_ok=", all_ok);
    }
    {
        auto works = std::vector<coop::Async<bool>>();
        works.emplace_back(some_heavy_work_result());
        works.emplace_back(some_heavy_work_result());
        const auto results = co_await coop::run_vec<coop::Async<bool>>(std::move(works));
        const auto all_ok  = std::ranges::all_of(results, [](const bool v) { return v; });
        PRINT("result 1=", results[0], " 2=", results[1], " all_ok=", all_ok);
    }
    co_return 0;
}

auto single_event_test_waiter(coop::SingleEvent& event) -> coop::Async<void> {
    PRINT("1");
    co_await event;
    PRINT("2");
    co_await event;
    PRINT("3");
    co_await event;

    PRINT("4");
    co_await event;
    PRINT("5");
    co_await event;
    PRINT("6");
    co_await event;

    PRINT("done");
}

auto single_event_test_notifier(coop::SingleEvent& event) -> coop::Async<void> {
    // notify after await
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
    event.notify();

    // notify before await
    event.notify();
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
}

auto event_cancel_test() -> coop::Async<void> {
    auto event  = coop::SingleEvent();
    auto handle = coop::TaskHandle();
    co_await coop::run_args(
        [](coop::SingleEvent& event) -> coop::Async<void> {
            co_await event;
        }(event))
        .detach({&handle});
    co_await coop::sleep(std::chrono::seconds(1));
    handle.cancel();
    event.notify();
    co_return;
}

auto multi_event_test() -> coop::Async<void> {
    auto event  = coop::MultiEvent();
    auto waiter = [](coop::MultiEvent& event) -> coop::Async<void> {
        for(auto i = 0; i < 3; i += 1) {
            co_await event;
            PRINT("notified");
        }
    };
    co_await coop::run_args(waiter(event), waiter(event), waiter(event)).detach();

    for(auto i = 0; i < 3; i += 1) {
        co_await delay_secs(1);
        event.notify();
    }
}

auto io_test_reader(coop::Pipe& pipe) -> coop::Async<void> {
    auto buf = std::array<char, 16>();
    while(true) {
        const auto result = co_await coop::wait_for_file(pipe.producer(), true, false);
        ASSERT(result.read && !result.error);
        const auto len = pipe.read(buf.data(), buf.size());
        ASSERT(len >= 0);
        const auto str = std::string_view(buf.data(), size_t(len));
        PRINT("read: ", str);
        if(str == "quit") {
            co_return;
        }
    }
}

auto io_test_writer(coop::Pipe& pipe) -> coop::Async<void> {
    co_await delay_secs(1);
    pipe.write("hello", 5);
    co_await delay_secs(1);
    pipe.write("world", 5);
    co_await delay_secs(1);
    pipe.write("quit", 4);
}

auto detach_test() -> coop::Async<void> {
    PRINT();
    auto event = coop::SingleEvent();
    PRINT();
    co_await coop::run_args(single_event_test_notifier(event)).detach();
    PRINT();
    co_await coop::run_args(single_event_test_waiter(event));
    PRINT();
}

auto user_handle_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(3)).detach({&handle});
    while(true) {
        if(handle.destroyed) {
            PRINT("exitted");
            break;
        } else {
            PRINT("still running");
            co_await delay_secs(1);
        }
    }
}

auto task_cancel_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(10)).detach({&handle});
    PRINT("waiting for job");
    co_await (delay_secs(1));
    if(!handle.destroyed) {
        PRINT("still running, canceling");
        handle.cancel();
    }
}

auto thread_event_test() -> coop::Async<void> {
    auto event  = coop::ThreadEvent();
    auto thread = std::thread([&event] {
        for(auto i = 0; i < 3; i += 1) {
            sleep_secs(1);
            event.notify();
        }
    });
    PRINT("thread started");
    for(auto i = 0; i < 3; i += 1) {
        co_await event;
        PRINT("notified");
    }

    event.notify();
    event.notify();
    event.notify();
    ASSERT(co_await event == 3);

    PRINT("done");
    thread.join();
}

auto os_thread_test() -> coop::Async<void> {
    // without return value
    PRINT("launching blocking function");
    co_await coop::run_blocking([](int seconds) { sleep_secs(seconds); }, 1);
    // with return value
    PRINT("launching blocking function");
    const auto result = co_await coop::run_blocking([](int seconds) { sleep_secs(seconds); return true; }, 1);
    PRINT("result=", result);
}

template <size_t n_threads>
auto push_task_from_other_thread_test() -> coop::Async<void> {
    auto& runner  = *co_await coop::reveal_runner();
    auto  blocker = std::conditional_t<n_threads == 1, coop::Blocker, coop::RecursiveBlocker>();
    blocker.start(runner);
    auto threads = std::array<std::thread, n_threads>();
    for(auto& thread : threads) {
        thread = std::thread([&runner, &blocker]() {
            const auto tid = std::this_thread::get_id();
            for(auto i = 0; i < 3; i += 1) {
                blocker.block();
                runner.push_task([](decltype(tid) tid) -> coop::Async<void> {
                    PRINT("spawned by thread ", tid);
                    co_return;
                }(tid));
                blocker.unblock();
            }
        });
    }
    co_await coop::run_blocking([&threads]() {
        for(auto& thread : threads) {
            thread.join();
        }
    });
    blocker.stop();
    PRINT("done");
}

auto task_injector_test() -> coop::Async<void> {
    auto& runner   = *co_await coop::reveal_runner();
    auto  injector = coop::TaskInjector(runner);
    auto  thread   = std::thread([&injector]() {
        const auto tid = std::this_thread::get_id();
        // without return value
        injector.inject_task(
            [](decltype(tid) tid) -> coop::Async<void> {
                PRINT("spawned by thread ", tid);
                co_return;
            }(tid));
        // with return value
        const auto ret = injector.inject_task(
            [](decltype(tid) tid) -> coop::Async<int> {
                PRINT("spawned by thread ", tid);
                co_return 0x1d6b;
            }(tid));
        PRINT("result=", ret);
    });
    co_await coop::run_blocking([&thread]() { thread.join(); });
    PRINT("done");
}

auto main(const int argc, const char* const* argv) -> int {
    if(argc == 2) {
        speed_rate = std::strtod(argv[1], NULL);
        ASSERT(speed_rate != 0.0);
    }

    auto runner = coop::Runner();

    PRINT("==== delay ====");
    runner.push_task(sleep_test());
    runner.run();

    PRINT("==== single event await ====");
    auto event = coop::SingleEvent();
    runner.push_task(single_event_test_waiter(event), single_event_test_notifier(event));
    runner.run();

    PRINT("==== single event cancel ====");
    runner.push_task(event_cancel_test());
    runner.run();

    PRINT("==== multi event ====");
    runner.push_task(multi_event_test());
    runner.run();

    PRINT("==== io await ====");
    auto pipe = coop::Pipe();
    runner.push_task(io_test_reader(pipe), io_test_writer(pipe));
    runner.run();

    PRINT("==== detached task ====");
    runner.push_task(detach_test());
    runner.run();

    PRINT("==== task handle ====");
    runner.push_task(user_handle_test());
    runner.run();

    PRINT("==== task cancel ====");
    runner.push_task(task_cancel_test());
    runner.run();

    PRINT("==== thread-safe event ====");
    runner.push_task(thread_event_test());
    runner.run();

    PRINT("==== blocking adapter ====");
    runner.push_task(os_thread_test());
    runner.run();

    PRINT("==== push task from other thread ====");
    runner.push_task(push_task_from_other_thread_test<1>());
    runner.run();

    PRINT("==== push task from multiple other thread ====");
    runner.push_task(push_task_from_other_thread_test<3>());
    runner.run();

    PRINT("==== inject task from other thread ====");
    runner.push_task(task_injector_test());
    runner.run();

    return 0;
}
