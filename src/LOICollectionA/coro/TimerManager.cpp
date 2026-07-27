#include <memory>
#include <string>
#include <functional>
#include <shared_mutex>
#include <unordered_map>

#include "ll/api/coro/Executor.h"
#include "ll/api/data/CancellableCallback.h"

#include "LOICollectionA/coro/TimerManager.h"

TimerManager::TimerManager(const ll::coro::Executor& executor) : mExecutor(std::ref(executor)) {}
TimerManager::~TimerManager() {
    this->cancelAll();
}

void TimerManager::schedule(std::string id, ll::coro::Duration delay, std::function<void()> callback) {
    std::unique_lock lock(this->mMutex);

    this->cancelUnlocked(id);

    auto wrapped = [self = shared_from_this(), id](std::function<void()> userCallback) -> void {
        userCallback();
        
        self->remove(id);
    };

    auto handle = this->mExecutor.get().executeAfter([wrapped, capture0 = std::move(callback)]() -> void {
        wrapped(capture0);
    }, delay);
    
    mTimers.emplace(std::move(id), handle);
}

void TimerManager::loopSchedule(std::string id, ll::coro::Duration delay, std::function<void()> callback) {
    std::unique_lock lock(this->mMutex);

    this->cancelUnlocked(id);

    auto repeater = std::make_shared<std::function<void()>>();
    *repeater = [self = shared_from_this(), id, delay, cb = std::move(callback), repeater]() {
        cb();

        std::unique_lock lock(self->mMutex);

        if (self->mTimers.find(id) == self->mTimers.end())
            return;

        auto handle = self->mExecutor.get().executeAfter(*repeater, delay);

        self->mTimers[id] = handle;
    };

    auto handle = this->mExecutor.get().executeAfter(*repeater, delay);

    mTimers.emplace(std::move(id), handle);
}

bool TimerManager::cancel(const std::string& id) {
    std::unique_lock lock(this->mMutex);

    return this->cancelUnlocked(id);
}

bool TimerManager::has(const std::string& id) const {
    std::shared_lock lock(this->mMutex);

    return this->mTimers.find(id) != this->mTimers.end();
}

void TimerManager::cancelAll() {
    std::unique_lock lock(this->mMutex);

    this->cancelAllUnlocked();
}

void TimerManager::remove(const std::string& id) {
    std::unique_lock lock(this->mMutex);

    this->mTimers.erase(id);
}

void TimerManager::setExecutor(ll::coro::NonNullExecutorRef executor) {
    std::unique_lock lock(this->mMutex);

    this->cancelAllUnlocked();

    this->mExecutor = std::ref(executor);
}

ll::coro::NonNullExecutorRef TimerManager::getExecutor() const {
    std::shared_lock lock(this->mMutex);

    return this->mExecutor;
}

bool TimerManager::cancelUnlocked(const std::string& id) {
    auto it = this->mTimers.find(id);
    if (it != this->mTimers.end()) {
        it->second->cancel();

        mTimers.erase(it);
        return true;
    }

    return false;
}

void TimerManager::cancelAllUnlocked() {
    for (auto& [id, ptr] : this->mTimers)
        ptr->cancel();

    this->mTimers.clear();
}
