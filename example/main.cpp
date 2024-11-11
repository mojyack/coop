#include <algorithm>
#include <chrono>
#include <cstdlib>

#include <coop/assert.hpp>
#include <coop/blocker.hpp>
#include <coop/generator.hpp>
#include <coop/io.hpp>
#include <coop/multi-event.hpp>
#include <coop/parallel.hpp>
#include <coop/pipe.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/single-event.hpp>
#include <coop/thread.hpp>
#include <coop/timer.hpp>
#include <unistd.h>

auto speed_rate = 1.0;

auto delay_secs(int seconds) -> coop::Async<int> {
    coop::line_print("delay ", seconds);
    co_await coop::sleep(std::chrono::milliseconds(size_t(1000 * seconds / speed_rate)));
    co_return seconds;
}

auto sleep_secs(int seconds) -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds(size_t(1000 * seconds / speed_rate)));
}

auto some_heavy_work() -> coop::Async<void> {
    coop::line_print("doing some work...");
    co_await delay_secs(3);
    co_return;
}

auto some_heavy_work_result() -> coop::Async<bool> {
    coop::line_print("doing some work...");
    co_await delay_secs(3);
    co_return true;
}

auto sleep_test() -> coop::Async<int> {
    {
        coop::line_print("count = ", co_await delay_secs(3));
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
        coop::line_print("result 1=", std::get<0>(results), " 2=", std::get<1>(results), " all_ok=", all_ok);
    }
    {
        auto works = std::vector<coop::Async<bool>>();
        works.emplace_back(some_heavy_work_result());
        works.emplace_back(some_heavy_work_result());
        const auto results = co_await coop::run_vec<coop::Async<bool>>(std::move(works));
        const auto all_ok  = std::ranges::all_of(results, [](const bool v) { return v; });
        coop::line_print("result 1=", results[0], " 2=", results[1], " all_ok=", all_ok);
    }
    co_return 0;
}

auto single_event_test_waiter(coop::SingleEvent& event) -> coop::Async<void> {
    coop::line_print("1");
    co_await event;
    coop::line_print("2");
    co_await event;
    coop::line_print("3");
    co_await event;

    coop::line_print("4");
    co_await event;
    coop::line_print("5");
    co_await event;
    coop::line_print("6");
    co_await event;

    coop::line_print("done");
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
            coop::line_print("notified");
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
        coop::assert(result.read && !result.error);
        const auto len = pipe.read(buf.data(), buf.size());
        coop::assert(len >= 0);
        const auto str = std::string_view(buf.data(), size_t(len));
        coop::line_print("read: ", str);
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
    coop::line_print();
    auto event = coop::SingleEvent();
    coop::line_print();
    co_await coop::run_args(single_event_test_notifier(event)).detach();
    coop::line_print();
    co_await coop::run_args(single_event_test_waiter(event));
    coop::line_print();
}

auto user_handle_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(3)).detach({&handle});
    while(true) {
        if(handle.destroyed) {
            coop::line_print("exitted");
            break;
        } else {
            coop::line_print("still running");
            co_await delay_secs(1);
        }
    }
}

auto task_cancel_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(10)).detach({&handle});
    coop::line_print("waiting for job");
    co_await (delay_secs(1));
    if(!handle.destroyed) {
        coop::line_print("still running, canceling");
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
    coop::line_print("thread started");
    for(auto i = 0; i < 3; i += 1) {
        co_await event;
        coop::line_print("notified");
    }

    event.notify();
    event.notify();
    event.notify();
    coop::assert(co_await event == 3);

    coop::line_print("done");
    thread.join();
}

auto os_thread_test() -> coop::Async<void> {
    // without return value
    coop::line_print("launching blocking function");
    co_await coop::run_blocking([](int seconds) { sleep_secs(seconds); }, 1);
    // with return value
    coop::line_print("launching blocking function");
    const auto result = co_await coop::run_blocking([](int seconds) { sleep_secs(seconds); return true; }, 1);
    coop::line_print("result=", result);
}

auto push_task_from_other_thread_test() -> coop::Async<void> {
    auto& runner  = *co_await coop::reveal_runner();
    auto  blocker = coop::Blocker();
    blocker.start(runner);
    auto other_thread = std::thread([&runner, &blocker]() {
        const auto tid = std::this_thread::get_id();
        for(auto i = 0; i < 3; i += 1) {
            blocker.block();
            runner.push_task([](decltype(tid) tid) -> coop::Async<void> {
                coop::line_print("spawned by thread ", tid);
                co_return;
            }(tid));
            blocker.unblock();
        }
    });
    co_await coop::run_blocking([&other_thread]() {other_thread.join(); return true; });
    blocker.stop();
    coop::line_print("done");
}

auto main(const int argc, const char* const* argv) -> int {
    if(argc == 2) {
        speed_rate = std::strtod(argv[1], NULL);
        coop::assert(speed_rate != 0.0);
    }

    auto runner = coop::Runner();

    coop::line_print("==== delay ====");
    runner.push_task(sleep_test());
    runner.run();

    coop::line_print("==== single event await ====");
    auto event = coop::SingleEvent();
    runner.push_task(single_event_test_waiter(event), single_event_test_notifier(event));
    runner.run();

    coop::line_print("==== single event cancel ====");
    runner.push_task(event_cancel_test());
    runner.run();

    coop::line_print("==== multi event ====");
    runner.push_task(multi_event_test());
    runner.run();

    coop::line_print("==== io await ====");
    auto pipe = coop::Pipe();
    runner.push_task(io_test_reader(pipe), io_test_writer(pipe));
    runner.run();

    coop::line_print("==== detached task ====");
    runner.push_task(detach_test());
    runner.run();

    coop::line_print("==== task handle ====");
    runner.push_task(user_handle_test());
    runner.run();

    coop::line_print("==== task cancel ====");
    runner.push_task(task_cancel_test());
    runner.run();

    coop::line_print("==== thread-safe event ====");
    runner.push_task(thread_event_test());
    runner.run();

    coop::line_print("==== blocking adapter ====");
    runner.push_task(os_thread_test());
    runner.run();

    coop::line_print("==== push task from other thread ====");
    runner.push_task(push_task_from_other_thread_test());
    runner.run();

    return 0;
}
