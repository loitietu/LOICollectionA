#include <memory>
#include <vector>
#include <algorithm>
#include <functional>

#include "ll/api/coro/Executor.h"
#include "ll/api/data/CancellableCallback.h"

#include "common/coro/MockExecutor.h"

MockExecutor::MockExecutor() : Executor("Mock"), mNow(TimePoint{}) {}

void MockExecutor::execute(std::function<void()> fn) const {
    fn();
}

std::shared_ptr<ll::data::CancellableCallback> MockExecutor::executeAfter(std::function<void()> fn, Duration delay) const {
    auto expire = this->mNow + delay;
    auto cb = std::make_shared<ll::data::CancellableCallback>(std::move(fn));

    this->mPending.emplace_back(expire, cb);
    return cb;
}

void MockExecutor::advanceTime(Duration delta) {
    this->mNow += delta;

    std::vector<std::shared_ptr<ll::data::CancellableCallback>> ready;
    this->mPending.erase(
        std::remove_if(this->mPending.begin(), this->mPending.end(),
            [&](const auto& item) {
                if (item.first <= this->mNow) {
                    ready.push_back(std::move(item.second));

                    return true;
                }
                return false;
            }),
        this->mPending.end());

    for (auto& cb : ready)
        cb->call();
}
