// Tiny header-only test harness — no external dep (Catch2/GTest aren't available
// in the dev sandbox; the headless core must compile with plain g++). Mirrors the
// role of tests/smoke_test.py: assert against a synthetic Steam dir, no real Steam.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace ss::test {

// Portable helpers so the test fixtures build on MSVC and g++ alike.
inline long procId() {
#if defined(_WIN32)
    return _getpid();
#else
    return getpid();
#endif
}

inline void setEnv(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

struct Case { std::string name; std::function<void()> fn; };

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }

struct Reg { Reg(const std::string& n, std::function<void()> fn) { registry().push_back({n, std::move(fn)}); } };

#define TEST_CASE(name) \
    static void name(); \
    static ::ss::test::Reg reg_##name(#name, name); \
    static void name()

struct AssertFail {};

inline void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::printf("    ASSERT FAILED: %s  (%s:%d)\n", expr, file, line);
        ++failures();
        throw AssertFail{};
    }
}

#define CHECK(cond) ::ss::test::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) ::ss::test::check((a) == (b), #a " == " #b, __FILE__, __LINE__)

inline int run_all() {
    int passed = 0;
    for (auto& c : registry()) {
        int before = failures();
        try { c.fn(); } catch (const AssertFail&) {}
        if (failures() == before) { ++passed; }
        else { std::printf("  [FAIL] %s\n", c.name.c_str()); }
    }
    std::printf("\n%d/%zu tests passed\n", passed, registry().size());
    return failures() == 0 ? 0 : 1;
}

}  // namespace ss::test
