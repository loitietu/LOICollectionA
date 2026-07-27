#include <gtest/gtest.h>

#include <chrono>

#include "common/coro/MockExecutor.h"

#include "LOICollectionA/coro/TimerManager.h"

using namespace std::chrono_literals;

class TimerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        executor_ = std::make_unique<MockExecutor>();
        manager_ = std::make_shared<TimerManager>(*executor_);
    }

    std::unique_ptr<MockExecutor> executor_;
    std::shared_ptr<TimerManager> manager_;
};

TEST_F(TimerManagerTest, ScheduleExecutesCallback) {
    bool called = false;
    manager_->schedule("t1", 100ms, [&]() -> void {
        called = true;
    });

    EXPECT_TRUE(manager_->has("t1"));

    executor_->advanceTime(100ms);

    EXPECT_TRUE(called);
    EXPECT_FALSE(manager_->has("t1"));
}

TEST_F(TimerManagerTest, CancelPreventsExecution) {
    bool called = false;
    manager_->schedule("t2", 100ms, [&]() -> void {
        called = true;
    });
    manager_->cancel("t2");

    executor_->advanceTime(200ms);

    EXPECT_FALSE(called);
    EXPECT_FALSE(manager_->has("t2"));
}

TEST_F(TimerManagerTest, OverwriteCancelsPrevious) {
    int value = 0;
    manager_->schedule("id", 50ms, [&]() -> void {
        value = 1;
    });
    manager_->schedule("id", 100ms, [&]() -> void {
        value = 2;
    });

    executor_->advanceTime(50ms);

    EXPECT_EQ(value, 0);

    executor_->advanceTime(50ms);
    
    EXPECT_EQ(value, 2);
}
