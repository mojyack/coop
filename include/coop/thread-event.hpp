#pragma once
#if defined(_WIN32)
#include "thread-event-windows.hpp"
#else
#include "thread-event-unix.hpp"
#endif
