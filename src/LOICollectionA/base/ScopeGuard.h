#pragma once

#include <utility>
#include <exception> 

template <typename Callable>
class ScopeGuard {
public:
    explicit ScopeGuard(Callable&& action) noexcept
        : mAction(std::forward<Callable>(action)) {}

    ~ScopeGuard() noexcept {
        if (mActive)
            try { mAction(); } catch (...) {}
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : mAction(std::move(other.mAction)),
          mActive(std::exchange(other.mActive, false)) {}

    void dismiss() noexcept {
        mActive = false;
    }

private:
    Callable mAction;

    bool mActive = true; 
};

template <typename Callable>
class SuccessGuard {
public:
    explicit SuccessGuard(Callable&& action) noexcept
        : mAction(std::forward<Callable>(action)),
          mExceptionCount(std::uncaught_exceptions()) {}

    ~SuccessGuard() noexcept {
        if (mActive && std::uncaught_exceptions() <= mExceptionCount)
            try { mAction(); } catch (...) {}
    }

    void dismiss() noexcept {
        mActive = false;
    }

    SuccessGuard(const SuccessGuard&) = delete;
    SuccessGuard& operator=(const SuccessGuard&) = delete;

    SuccessGuard(SuccessGuard&& other) noexcept
        : mAction(std::move(other.mAction)),
          mExceptionCount(other.mExceptionCount),
          mActive(std::exchange(other.mActive, false)) {}

private:
    Callable mAction;

    int mExceptionCount;
    bool mActive = true;
};

template <typename Callable>
class FailureGuard {
public:
    explicit FailureGuard(Callable&& action) noexcept
        : mAction(std::forward<Callable>(action)),
          mExceptionCount(std::uncaught_exceptions()) {}

    ~FailureGuard() noexcept {
        if (mActive && std::uncaught_exceptions() > mExceptionCount)
            try { mAction(); } catch (...) {}
    }

    void dismiss() noexcept { 
        mActive = false;
    }

    FailureGuard(const FailureGuard&) = delete;
    FailureGuard& operator=(const FailureGuard&) = delete;

    FailureGuard(FailureGuard&& other) noexcept
        : mAction(std::move(other.mAction)),
          mExceptionCount(other.mExceptionCount),
          mActive(std::exchange(other.mActive, false)) {}

private:
    Callable mAction;

    int mExceptionCount;
    bool mActive = true;
};

template <typename Callable>
auto make_scope_guard(Callable&& action) {
    return ScopeGuard<std::decay_t<Callable>>(
        std::forward<Callable>(action)
    );
}

template <typename Callable>
auto make_success_guard(Callable&& action) {
    return SuccessGuard<std::decay_t<Callable>>(
        std::forward<Callable>(action)
    );
}

template <typename Callable>
auto make_failure_guard(Callable&& action) {
    return FailureGuard<std::decay_t<Callable>>(
        std::forward<Callable>(action)
    );
}