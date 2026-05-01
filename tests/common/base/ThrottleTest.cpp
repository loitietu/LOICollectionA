#include <gtest/gtest.h>

#include <chrono>

#include "LOICollectionA/base/Throttle.h"

using namespace std::chrono_literals;

TEST(ThrottleTest, AllowInitiallyTrue) {
    Throttle throttle(1s);

    EXPECT_TRUE(throttle.allow());
}

TEST(ThrottleTest, ConsecutiveCallsWithinInterval) {
    Throttle throttle(1s);

    ASSERT_TRUE(throttle.allow());
    EXPECT_FALSE(throttle.allow());
}

TEST(ThrottleTest, ResetEnablesAllow) {
    Throttle throttle(1s);

    throttle.allow();

    EXPECT_FALSE(throttle.allow());

    throttle.reset();

    EXPECT_TRUE(throttle.allow());
}

TEST(ThrottleTest, OperatorCallable) {
    Throttle throttle(1s);

    int counter = 0;
    bool executed = throttle([&] { ++counter; });

    EXPECT_TRUE(executed);
    EXPECT_EQ(counter, 1);

    executed = throttle([&] { ++counter; });

    EXPECT_FALSE(executed);
    EXPECT_EQ(counter, 1);
}

TEST(ThrottleTest, GetIntervalAndLastExecution) {
    Throttle throttle(500ms);

    EXPECT_EQ(throttle.getInterval(), 500ms);

    auto before = std::chrono::steady_clock::now();

    throttle.allow();
    
    auto last = throttle.getLastExecution();
    EXPECT_GE(last, before);
}
