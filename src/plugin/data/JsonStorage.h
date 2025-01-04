#ifndef JSONUTILS_H
#define JSONUTILS_H

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
    T get(std::string_view key) const {
        return this->d_json.at(key).get<T>();
    }

    template <typename T>
    void set(std::string_view key, T value) {
        this->d_json[key] = value;
    }

    LOICOLLECTION_A_API   void write(nlohmann::ordered_json& json);
    LOICOLLECTION_A_API   void save() const;
    
    LOICOLLECTION_A_NDAPI std::vector<std::string> keys() const;

    LOICOLLECTION_A_NDAPI std::string toString(int indent = 0) const;
    LOICOLLECTION_A_NDAPI nlohmann::ordered_json toJson(std::string_view key) const;
    LOICOLLECTION_A_NDAPI nlohmann::ordered_json toJson() const;
private:
    std::filesystem::path d_path;
    nlohmann::ordered_json d_json = nlohmann::ordered_json::object();
};

template LOICOLLECTION_A_UNAPI void JsonStorage::set<int>(std::string_view, int);
template LOICOLLECTION_A_UNAPI void JsonStorage::set<bool>(std::string_view, bool);
template LOICOLLECTION_A_UNAPI void JsonStorage::set<float>(std::string_view, float);
template LOICOLLECTION_A_UNAPI void JsonStorage::set<std::string>(std::string_view, std::string);
template LOICOLLECTION_A_UNAPI void JsonStorage::set<nlohmann::ordered_json>(std::string_view, nlohmann::ordered_json);

template LOICOLLECTION_A_UNAPI int JsonStorage::get<int>(std::string_view) const;
template LOICOLLECTION_A_UNAPI bool JsonStorage::get<bool>(std::string_view) const;
template LOICOLLECTION_A_UNAPI float JsonStorage::get<float>(std::string_view) const;
template LOICOLLECTION_A_UNAPI std::string JsonStorage::get<std::string>(std::string_view) const;
template LOICOLLECTION_A_UNAPI nlohmann::ordered_json JsonStorage::get<nlohmann::ordered_json>(std::string_view) const;

#endif