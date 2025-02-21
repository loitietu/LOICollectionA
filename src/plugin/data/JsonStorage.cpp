#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonStorage.h"

JsonStorage::JsonStorage(const std::filesystem::path& path) : d_path(path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream file(path);
        file << "{}";
        file.close();
    }
    std::ifstream file(path);
    if (file.is_open()) {
        file >> this->d_json;
        file.close();
    }
}

bool JsonStorage::remove(std::string_view key) {
    return this->d_json.erase(key) > 0;
}

bool JsonStorage::has(std::string_view key) const {
    return this->d_json.contains(key);
}

bool JsonStorage::isEmpty() const {
    return this->d_json.empty();
}

void JsonStorage::write(const nlohmann::ordered_json& json) {
    this->d_json = json;
}

void JsonStorage::save() const {
    std::ofstream file(this->d_path);
    if (file.is_open()) {
        file << this->d_json.dump(4);
        file.close();
    }
}

std::vector<std::string> JsonStorage::keys() const {
    std::vector<std::string> result;
    result.reserve(this->d_json.size());
    for (auto& el : this->d_json.items())
        result.emplace_back(el.key());
    return result;
}

std::string JsonStorage::toString(int indent) const {
    return this->d_json.dump(indent);
}

nlohmann::ordered_json JsonStorage::toJson(std::string_view key) const {
    return this->d_json.value(key, nlohmann::ordered_json());
}

nlohmann::ordered_json JsonStorage::toJson() const {
    return this->d_json;
}