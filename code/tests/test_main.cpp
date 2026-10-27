// test_main.cpp

#include "mini_test.h"

TEST(Sanity) {
    CHECK(1 + 1 == 2);
}

int main() {
    return RunAllTests();
}
