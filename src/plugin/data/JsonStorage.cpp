#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonStorage.h"

JsonStorage::JsonStorage(const std::filesystem::path& path) : d_path(path) {
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

bool JsonStorage::remove(std::string_view key) {
    return this->d_json.erase(key) > 0;
}

bool JsonStorage::has(std::string_view key) const {
    return this->d_json.contains(key);
}

bool JsonStorage::isEmpty() const {
    return this->d_json.empty();
}

void JsonStorage::write(nlohmann::ordered_json& json) {
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
    std::vector<std::string> keyl;
    for (auto it = this->d_json.begin(); it != this->d_json.end(); ++it)
        keyl.push_back(it.key());
    return keyl;
}

std::string JsonStorage::toString(int indent) const {
    return this->d_json.dump(indent);
}

nlohmann::ordered_json JsonStorage::toJson(std::string_view key) const {
    if (!has(key))
        return nlohmann::ordered_json();
    return this->d_json.at(key);
}

nlohmann::ordered_json JsonStorage::toJson() const {
    return this->d_json;
}