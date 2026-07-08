#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include <functional>

#include "ll/api/coro/Executor.h"
#include "ll/api/data/CancellableCallback.h"

class MockExecutor : public ll::coro::Executor {
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

public:
    MockExecutor();

    void execute(std::function<void()> fn) const override;

    std::shared_ptr<ll::data::CancellableCallback> executeAfter(std::function<void()> fn, Duration delay) const override;

    void advanceTime(Duration delta);

private:
    TimePoint mNow;

    mutable std::vector<std::pair<TimePoint, std::shared_ptr<ll::data::CancellableCallback>>> mPending;
};
