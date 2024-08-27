#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonUtils.h"

JsonUtils::JsonUtils(const std::filesystem::path& path) {
    this->d_path = path;
    if (!std::filesystem::exists(path)) {
        std::ofstream newfile(path);
        newfile << this->d_json.dump();
        newfile.close();
    }
    std::ifstream file(path);
    if (file.is_open()) {
        file >> this->d_json;
        file.close();
    }
}

bool JsonUtils::del(std::string_view key) {
    return this->d_json.erase(key) > 0;
}

template<typename T>
T JsonUtils::get(std::string_view key) {
    return this->d_json.at(key).get<T>();
}

bool JsonUtils::has(std::string_view key) {
    return this->d_json.contains(key);
}

bool JsonUtils::empty() {
    return this->d_json.empty();
}

bool JsonUtils::reset() {
    this->d_json.clear();
    return true;
}

void JsonUtils::set(std::string_view key, const std::string& value) {
    this->d_json[key] = value;
}

void JsonUtils::set(std::string_view key, nlohmann::ordered_json& value) {
    this->d_json[key] = value;
}

void JsonUtils::save() {
    std::ofstream file(this->d_path);
    if (file.is_open()) {
        file << this->d_json.dump(4);
        file.close();
    }
}

std::vector<std::string> JsonUtils::keys() {
    std::vector<std::string> keyl;
    for (auto& [key, _] : this->d_json.items()) {
        keyl.push_back(key);
    }
    return keyl;
}

size_t JsonUtils::size() {
    return this->d_json.size();
}

std::string JsonUtils::toString(int indent) {
    return this->d_json.dump(indent);
}

nlohmann::ordered_json JsonUtils::toJson(std::string key) {
    return this->d_json[key];
}

nlohmann::ordered_json JsonUtils::toJson() {
    return this->d_json;
}