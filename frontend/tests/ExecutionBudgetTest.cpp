#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "CommonTest.h"

using namespace LOICollection::frontend;

TEST(ExecutionBudgetTest, InfiniteLoopHitsInstructionBudget) {
    EXPECT_THROW(eval("while (true) [ ]"), std::runtime_error);
}

TEST(ExecutionBudgetTest, InfiniteRecursionHitsCallDepthLimit) {
    EXPECT_THROW(eval("func f() { return f(); } f()"), std::runtime_error);
}

TEST(ExecutionBudgetTest, BoundedLoopWithinBudget) {
    /* 0..10000 is an exclusive range (0..9999), so the sum is 49995000. */
    EXPECT_EQ(eval("total = 0; for (i in 0..10000) [ total += i; ]; total"), "49995000");
}

TEST(ExecutionBudgetTest, DeepButBoundedRecursionSucceeds) {
    EXPECT_EQ(eval(
        "func down(n: int) -> int { "
        "    if (n == 0) [ return 0; ]; "
        "    return down(n - 1); "
        "} "
        "down(100)"),
        "0");
}
