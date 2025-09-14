#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <type_traits>
#include <filesystem>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "base/Macro.h"

class JsonStorage {
public:
    LOICOLLECTION_A_API   explicit JsonStorage(std::filesystem::path path);
    LOICOLLECTION_A_API   ~JsonStorage() = default;

    LOICOLLECTION_A_API   bool remove(std::string_view key);
    LOICOLLECTION_A_API   bool remove_ptr(std::string_view ptr);
    LOICOLLECTION_A_NDAPI bool has(std::string_view key) const;
    LOICOLLECTION_A_NDAPI bool has_ptr(std::string_view ptr) const; 

    LOICOLLECTION_A_NDAPI nlohmann::ordered_json& get();

    LOICOLLECTION_A_NDAPI std::vector<std::string> keys() const;

    template <typename T>
    requires std::is_default_constructible<T>::value
    [[nodiscard]] T get(std::string_view key, const T& defaultValue = {}) const {
        return d_json.value(key, defaultValue);
    }

    template <typename T>
    requires std::is_default_constructible<T>::value
    [[nodiscard]] T get_ptr(std::string_view ptr, const T& defaultValue = {}) const {
        nlohmann::json_pointer<std::string> ptrs = nlohmann::json_pointer<std::string>(std::string(ptr));
        return d_json.contains(ptrs) ? d_json.at(ptrs).get<T>() : defaultValue;
    }

    template <typename T>
    void set(std::string_view key, T value) {
        d_json[key] = std::forward<T>(value);
    }

    template <typename T>
    void set_ptr(std::string_view ptr, T value) {
        nlohmann::json_pointer<std::string> ptrs = nlohmann::json_pointer<std::string>(std::string(ptr));
        d_json[ptrs] = std::forward<T>(value);
    }

    LOICOLLECTION_A_API   void save() const;

private:
    std::filesystem::path d_path;
    nlohmann::ordered_json d_json;
};
