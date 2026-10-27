// mini_test.h
//
// SURI: tiny unit test helper for code that has no D3D / window dependencies.
//   TEST(Name) { CHECK(expr); }
// Every TEST registers itself; RunAllTests() prints PASS/FAIL per test and returns
// the number of failed tests (used as the process exit code).

#pragma once

#include <stdio.h>

struct MiniTest {
    const char *name;
    void (*fn)();
    MiniTest *next;
};

inline MiniTest *&MiniTestHead() { static MiniTest *head = nullptr; return head; }
inline int &MiniTestFailures() { static int failures = 0; return failures; }

struct MiniTestRegistrar {
    MiniTestRegistrar(MiniTest *t) {
        // append so tests run in source order
        MiniTest **pp = &MiniTestHead();
        while (*pp) pp = &(*pp)->next;
        *pp = t;
    }
};

#define TEST(name) \
    static void name(); \
    static MiniTest name##_test = { #name, &name, nullptr }; \
    static MiniTestRegistrar name##_registrar(&name##_test); \
    static void name()

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            printf("  check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            MiniTestFailures()++; \
        } \
    } while (0)

inline int RunAllTests() {
    int failedTests = 0;
    for (MiniTest *t = MiniTestHead(); t; t = t->next) {
        int before = MiniTestFailures();
        t->fn();
        if (MiniTestFailures() != before) {
            printf("FAIL %s\n", t->name);
            failedTests++;
        } else {
            printf("PASS %s\n", t->name);
        }
    }
    printf("%d failed\n", failedTests);
    return failedTests;
}
