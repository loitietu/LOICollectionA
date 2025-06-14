#pragma once

#include <utility>
#include <exception> 

template <typename Callable>
class ScopeGuard {
public:
    explicit ScopeGuard(Callable&& action) noexcept
        : action_(std::forward<Callable>(action)) {}

    ~ScopeGuard() noexcept {
        if (active_)
            try { action_(); } catch (...) {}
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& other) noexcept
        : action_(std::move(other.action_)),
          active_(std::exchange(other.active_, false)) {}

    void dismiss() noexcept {
        active_ = false;
    }

private:
    Callable action_;
    bool active_ = true; 
};

template <typename Callable>
class SuccessGuard {
public:
    explicit SuccessGuard(Callable&& action) noexcept
        : action_(std::forward<Callable>(action)),
          exception_count_(std::uncaught_exceptions()) {}

    ~SuccessGuard() noexcept {
        if (active_ && std::uncaught_exceptions() <= exception_count_)
            try { action_(); } catch (...) {}
    }

    void dismiss() noexcept {
        active_ = false;
    }

    SuccessGuard(const SuccessGuard&) = delete;
    SuccessGuard& operator=(const SuccessGuard&) = delete;

    SuccessGuard(SuccessGuard&& other) noexcept
        : action_(std::move(other.action_)),
          exception_count_(other.exception_count_),
          active_(std::exchange(other.active_, false)) {}

private:
    Callable action_;
    int exception_count_;
    bool active_ = true;
};

template <typename Callable>
class FailureGuard {
public:
    explicit FailureGuard(Callable&& action) noexcept
        : action_(std::forward<Callable>(action)),
          exception_count_(std::uncaught_exceptions()) {}

    ~FailureGuard() noexcept {
        if (active_ && std::uncaught_exceptions() > exception_count_)
            try { action_(); } catch (...) {}
    }

    void dismiss() noexcept { 
        active_ = false;
    }

    FailureGuard(const FailureGuard&) = delete;
    FailureGuard& operator=(const FailureGuard&) = delete;

    FailureGuard(FailureGuard&& other) noexcept
        : action_(std::move(other.action_)),
          exception_count_(other.exception_count_),
          active_(std::exchange(other.active_, false)) {}

private:
    Callable action_;
    int exception_count_;
    bool active_ = true;
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