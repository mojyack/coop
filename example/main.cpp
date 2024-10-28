#include <chrono>
#include <source_location>

#include <coop/event.hpp>
#include <coop/generator.hpp>
#include <coop/io.hpp>
#include <coop/parallel.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/thread.hpp>
#include <coop/timer.hpp>
#include <unistd.h>

template <class... Args>
struct line_print {
    line_print(Args&&... args, const std::source_location location = std::source_location::current()) {
        ((std::cout << "line " << location.line() << ": ") << ... << args) << std::endl;
    }
};

template <class... Args>
line_print(Args&&... args) -> line_print<Args...>;

auto delay_secs(int seconds) -> coop::Async<int> {
    line_print("delay ", seconds);
    co_await coop::sleep(std::chrono::seconds(seconds));
    co_return seconds;
}

auto some_heavy_work() -> coop::Async<void> {
    line_print("doing some work...");
    co_await delay_secs(3);
    co_return;
}

auto some_heavy_work_result() -> coop::Async<bool> {
    line_print("doing some work...");
    co_await delay_secs(3);
    co_return true;
}

auto sleep_test() -> coop::Async<int> {
    {
        line_print("count = ", co_await delay_secs(3));
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
        line_print("result 1=", std::get<0>(results), " 2=", std::get<1>(results), " all_ok=", all_ok);
    }
    {
        auto works = std::vector<coop::Async<bool>>();
        works.emplace_back(some_heavy_work_result());
        works.emplace_back(some_heavy_work_result());
        const auto results = co_await coop::run_vec<coop::Async<bool>>(std::move(works));
        const auto all_ok  = std::ranges::all_of(results, [](const bool v) { return v; });
        line_print("result 1=", results[0], " 2=", results[1], " all_ok=", all_ok);
    }
    co_return 0;
}

auto event_test_waiter(coop::Event& event) -> coop::Async<void> {
    line_print("1");
    co_await event;
    line_print("2");
    co_await event;
    line_print("3");
    co_await event;
}

auto event_test_notifier(coop::Event& event) -> coop::Async<void> {
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
    event.notify();
    co_await delay_secs(1);
    event.notify();
}

auto io_test_reader(int fd) -> coop::Async<void> {
    auto buf = std::array<char, 16>();
    while(true) {
        const auto result = co_await coop::wait_for_file(fd, true, false);
        if(result.error || !result.read) {
            line_print("not read ready");
            continue;
        }
        const auto len = read(fd, buf.data(), buf.size());
        if(len <= 0) {
            line_print("read failed");
            continue;
        }
        const auto str = std::string_view(buf.data(), size_t(len));
        line_print("read: ", str);
        if(str == "quit") {
            co_return;
        }
    }
}

auto io_test_writer(int fd) -> coop::Async<void> {
    co_await delay_secs(1);
    write(fd, "hello", 5);
    co_await delay_secs(1);
    write(fd, "world", 5);
    co_await delay_secs(1);
    write(fd, "quit", 4);
}

auto detach_test() -> coop::Async<void> {
    line_print();
    auto event = coop::Event();
    line_print();
    co_await coop::run_args(event_test_notifier(event)).detach();
    line_print();
    co_await coop::run_args(event_test_waiter(event));
    line_print();
}

auto user_handle_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(3)).detach({&handle});
    while(true) {
        if(handle.destroyed) {
            line_print("exitted");
            break;
        } else {
            line_print("still running");
            co_await delay_secs(1);
        }
    }
}

auto task_cancel_test() -> coop::Async<void> {
    auto handle = coop::TaskHandle();
    co_await coop::run_args(delay_secs(10)).detach({&handle});
    line_print("waiting for job");
    co_await (delay_secs(1));
    if(!handle.destroyed) {
        line_print("still running, canceling");
        handle.cancel();
    }
}

auto os_thread_test() -> coop::Async<void> {
    const auto blocking_func = [](int seconds) { std::this_thread::sleep_for(std::chrono::seconds(seconds)); return true; };
    line_print("launching blocking function");
    const auto result = co_await coop::run_blocking(blocking_func, 3);
    line_print("done result=", result);
}

auto main() -> int {
    auto runner = coop::Runner();

    line_print("==== delay ====");
    runner.push_task(sleep_test());
    runner.run();

    line_print("==== event await ====");
    auto event = coop::Event();
    runner.push_task(event_test_waiter(event), event_test_notifier(event));
    runner.run();

    line_print("==== io await ====");
    auto fds = std::array<int, 2>();
    pipe(fds.data());
    runner.push_task(io_test_reader(fds[0]), io_test_writer(fds[1]));
    runner.run();

    line_print("==== detached task ====");
    runner.push_task(detach_test());
    runner.run();

    line_print("==== task handle ====");
    runner.push_task(user_handle_test());
    runner.run();

    line_print("==== task cancel ====");
    runner.push_task(task_cancel_test());
    runner.run();

    line_print("==== blocking adapter ====");
    runner.push_task(os_thread_test());
    runner.run();

    return 0;
}
