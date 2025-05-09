#include <exception>
#include <print>

#include "assert.hpp"

#pragma push_macro("PRINT")
#pragma push_macro("DEBUG")
#pragma push_macro("TRACE")
#pragma push_macro("PANIC")
#pragma push_macro("ASSERT")

// PRINT
#undef PRINT
#define PRINT(...)                                                 \
    {                                                              \
        std::print("{}:{} ", format_filename(__FILE__), __LINE__); \
        std::println(__VA_ARGS__);                                 \
    }

// DEBUG
#undef DEBUG
#if defined(COOP_DEBUG) || defined(COOP_TRACE)
#define DEBUG(...) PRINT(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

// TRACE
#undef TRACE
#if defined(COOP_TRACE)
#define TRACE(...) PRINT(__VA_ARGS__)
#else
#define TRACE(...)
#endif

#undef PANIC
#define PANIC(...)                                                                     \
    {                                                                                  \
        std::print(stderr, "panicked at {}:{} ", format_filename(__FILE__), __LINE__); \
        std::println(stderr, __VA_ARGS__);                                             \
        std::terminate();                                                              \
    }

#undef ASSERT
#define ASSERT(cond, ...)                                      \
    if(!(cond)) {                                              \
        PANIC("assertion failed" __VA_OPT__(" ") __VA_ARGS__); \
    }
