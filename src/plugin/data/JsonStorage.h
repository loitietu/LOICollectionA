#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "base/Macro.h"

class JsonStorage {
public:
    LOICOLLECTION_A_API   explicit JsonStorage(const std::filesystem::path& path);
    LOICOLLECTION_A_API   ~JsonStorage() = default;

    LOICOLLECTION_A_API   bool remove(std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::string_view key) const;
    LOICOLLECTION_A_NDAPI bool isEmpty() const;

    template <typename T>
    [[nodiscard]] T get(std::string_view key) const {
        return this->d_json.value(key, T());
    }

    template <typename T>
    void set(std::string_view key, T value) {
        this->d_json[key] = std::move(value);
    }

    LOICOLLECTION_A_API   void write(const nlohmann::ordered_json& json);
    LOICOLLECTION_A_API   void save() const;
    
    LOICOLLECTION_A_NDAPI std::vector<std::string> keys() const;

    LOICOLLECTION_A_NDAPI std::string toString(int indent = 0) const;
    LOICOLLECTION_A_NDAPI nlohmann::ordered_json toJson(std::string_view key) const;
    LOICOLLECTION_A_NDAPI nlohmann::ordered_json toJson() const;
private:
    std::filesystem::path d_path;
    nlohmann::ordered_json d_json;
};