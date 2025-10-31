#pragma once

#include <chrono>
#include <functional>

class Throttle {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::milliseconds;
    
    explicit Throttle(Duration interval) : mInterval(interval) {
        mLastExecution = Clock::now() - mInterval;
    }
    
    ~Throttle() = default;
    
    bool allow() {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - mLastExecution);
        
        if (elapsed >= mInterval) {
            mLastExecution = now;

            return true;
        }

        return false;
    }

    template<typename Func, typename... Args>
    bool operator()(Func&& func, Args&&... args) {
        if (allow()) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);

            return true;
        }

        return false;
    }

    void reset() {
        mLastExecution = Clock::now() - mInterval;
    }
    
    void setInterval(Duration interval) {
        mInterval = interval;
        
        reset();
    }
    
    [[nodiscard]] Duration getInterval() const {
        return mInterval;
    }

    [[nodiscard]] auto getLastExecution() const {
        return mLastExecution;
    }

private:
    Duration mInterval;
    std::chrono::time_point<Clock> mLastExecution;
};
