#pragma once
#include <cstdlib>
#include <iostream>
// Always active, including Release / NDEBUG builds.
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " << #expression << '\n'; \
    std::abort(); } } while (false)
