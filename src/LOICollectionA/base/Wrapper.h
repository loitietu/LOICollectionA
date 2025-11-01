#pragma once

#include <utility>
#include <concepts>

template <typename T>
class ReadOnlyWrapper {
private:
    T mData;

public:
    using value_type = T;
    
    template <typename U = T> requires std::constructible_from<T, U>
    explicit ReadOnlyWrapper(U&& value) : mData(std::forward<U>(value)) {}
    
    ReadOnlyWrapper(const ReadOnlyWrapper& other) : mData(other.mData) {}
    
    ReadOnlyWrapper(ReadOnlyWrapper&& other) noexcept : mData(std::move(other.mData)) {}
    
    ReadOnlyWrapper& operator=(const ReadOnlyWrapper&) = delete;
    ReadOnlyWrapper& operator=(ReadOnlyWrapper&&) = delete;
    
    const T& get() const & { return mData; }
    const T&& get() const && { return std::move(mData); }
    
    operator const T&() const & { return mData; }
    operator const T&&() const && { return std::move(mData); }
    
    const T* operator->() const noexcept { return &mData; }
    
    const T& operator*() const & noexcept { return mData; }
    const T&& operator*() const && noexcept { return std::move(mData); }
    
    template <typename Index>
    auto operator[](Index&& index) const -> decltype(mData[std::forward<Index>(index)]) {
        return mData[std::forward<Index>(index)];
    }
    
    template <typename... Args>
    auto operator()(Args&&... args) const -> decltype(std::invoke(mData, std::forward<Args>(args)...)) {
        return std::invoke(mData, std::forward<Args>(args)...);
    }
    
    template <typename U>
    bool operator==(const ReadOnlyWrapper<U>& other) const requires requires(const T& a, const U& b) { a == b; } {
        return mData == other.get();
    }
    
    template <typename U>
    auto operator<=>(const ReadOnlyWrapper<U>& other) const requires requires(const T& a, const U& b) { a <=> b; } {
        return mData <=> other.get();
    }
    
    template <typename U>
    bool operator==(const U& other) const requires requires(const T& a, const U& b) { a == b; } {
        return mData == other;
    }
};
