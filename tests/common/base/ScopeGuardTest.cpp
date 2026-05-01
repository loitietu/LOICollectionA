#include <gtest/gtest.h>

#include "LOICollectionA/base/ScopeGuard.h"

TEST(ScopeGuardTest, ScopeGuardExecutes) {
    int counter = 0;
    {
        auto guard = make_scope_guard([&] { ++counter; });
        EXPECT_EQ(counter, 0);
    }

    EXPECT_EQ(counter, 1);
}

TEST(ScopeGuardTest, DismissPreventsExecution) {
    int counter = 0;
    {
        auto guard = make_scope_guard([&] { ++counter; });
        guard.dismiss();
    }

    EXPECT_EQ(counter, 0);
}

TEST(ScopeGuardTest, MoveGuard) {
    int counter = 0;
    {
        auto guard1 = make_scope_guard([&] { ++counter; });
        auto guard2 = std::move(guard1);
    }

    EXPECT_EQ(counter, 1);
}

TEST(SuccessGuardTest, ExecutesOnNormalExit) {
    int counter = 0;
    {
        auto guard = make_success_guard([&] { ++counter; });
    }

    EXPECT_EQ(counter, 1);
}

TEST(SuccessGuardTest, DoesNotExecuteOnException) {
    int counter = 0;
    try {
        auto guard = make_success_guard([&] { ++counter; });
        throw std::runtime_error("test");
    } catch (...) {}

    EXPECT_EQ(counter, 0);
}

TEST(FailureGuardTest, ExecutesOnException) {
    int counter = 0;
    try {
        auto guard = make_failure_guard([&] { ++counter; });
        throw std::runtime_error("fail");
    } catch (...) {}

    EXPECT_EQ(counter, 1);
}

TEST(FailureGuardTest, DoesNotExecuteOnNormalExit) {
    int counter = 0;
    {
        auto guard = make_failure_guard([&] { ++counter; });
    }
    
    EXPECT_EQ(counter, 0);
}
