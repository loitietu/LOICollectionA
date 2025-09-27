#pragma once

#include <utility>
#include <concepts>

template<typename T>
class ReadOnlyWrapper {
private:
    T data_;

public:
    using value_type = T;
    
    template<typename U = T> requires std::constructible_from<T, U>
    explicit ReadOnlyWrapper(U&& value) : data_(std::forward<U>(value)) {}
    
    ReadOnlyWrapper(const ReadOnlyWrapper& other) : data_(other.data_) {}
    
    ReadOnlyWrapper(ReadOnlyWrapper&& other) noexcept : data_(std::move(other.data_)) {}
    
    ReadOnlyWrapper& operator=(const ReadOnlyWrapper&) = delete;
    ReadOnlyWrapper& operator=(ReadOnlyWrapper&&) = delete;
    
    const T& get() const & { return data_; }
    const T&& get() const && { return std::move(data_); }
    
    operator const T&() const & { return data_; }
    operator const T&&() const && { return std::move(data_); }
    
    const T* operator->() const noexcept { return &data_; }
    
    const T& operator*() const & noexcept { return data_; }
    const T&& operator*() const && noexcept { return std::move(data_); }
    
    template<typename Index>
    auto operator[](Index&& index) const -> decltype(data_[std::forward<Index>(index)]) {
        return data_[std::forward<Index>(index)];
    }
    
    template<typename... Args>
    auto operator()(Args&&... args) const -> decltype(std::invoke(data_, std::forward<Args>(args)...)) {
        return std::invoke(data_, std::forward<Args>(args)...);
    }
    
    template<typename U>
    bool operator==(const ReadOnlyWrapper<U>& other) const requires requires(const T& a, const U& b) { a == b; } {
        return data_ == other.get();
    }
    
    template<typename U>
    auto operator<=>(const ReadOnlyWrapper<U>& other) const requires requires(const T& a, const U& b) { a <=> b; } {
        return data_ <=> other.get();
    }
    
    template<typename U>
    bool operator==(const U& other) const requires requires(const T& a, const U& b) { a == b; } {
        return data_ == other;
    }
};
