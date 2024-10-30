#pragma once
#include <array>
#include <utility>

#include <unistd.h>

#include "assert.hpp"

namespace coop {
struct Pipe {
    std::array<int, 2> fds = {-1, -1};

    auto producer() -> int {
        return fds[0];
    }

    auto read(void* const buf, const size_t size) -> int {
        return ::read(fds[0], buf, size);
    }

    auto write(const void* const buf, const size_t size) -> int {
        return ::write(fds[1], buf, size);
    }

    Pipe(Pipe&& o) {
        fds = std::exchange(o.fds, {-1, -1});
    }

    Pipe() {
        assert(pipe(fds.data()) == 0, "errno=", errno, " ", strerror(errno));
    }

    ~Pipe() {
        if(fds[0] != -1) {
            close(fds[0]);
            close(fds[1]);
        }
    }
};
} // namespace coop
