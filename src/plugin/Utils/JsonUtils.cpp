#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonUtils.h"

JsonUtils::JsonUtils(const std::filesystem::path& path) : d_path(path) {
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

bool JsonUtils::remove(std::string_view key) {
    return this->d_json.erase(key) > 0;
}

bool JsonUtils::has(std::string_view key) const {
    return this->d_json.contains(key);
}

bool JsonUtils::isEmpty() const {
    return this->d_json.empty();
}

void JsonUtils::write(nlohmann::ordered_json& json) {
    this->d_json = json;
}

void JsonUtils::save() const {
    std::ofstream file(this->d_path);
    if (file.is_open()) {
        file << this->d_json.dump(4);
        file.close();
    }
}

std::vector<std::string> JsonUtils::keys() const {
    std::vector<std::string> keyl;
    for (auto& [key, _] : this->d_json.items()) {
        keyl.push_back(key);
    }
    return keyl;
}

std::string JsonUtils::toString(int indent) const {
    return this->d_json.dump(indent);
}

nlohmann::ordered_json JsonUtils::toJson(std::string_view key) const {
    if (!has(key))
        return nlohmann::ordered_json();
    return this->d_json.at(key);
}

nlohmann::ordered_json JsonUtils::toJson() const {
    return this->d_json;
}