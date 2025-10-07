#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "LOICollectionA/data/JsonStorage.h"

JsonStorage::JsonStorage(std::filesystem::path path) : d_path(std::move(path)) {
    if (std::filesystem::exists(d_path)) {
        if (std::ifstream file(d_path); file)
            file >> d_json;
    } else {
        std::filesystem::create_directories(d_path.parent_path());
        d_json = nlohmann::ordered_json::object();
        save();
    }
}

bool JsonStorage::remove(std::string_view key) {
    return d_json.erase(key) > 0;
}

bool JsonStorage::remove_ptr(std::string_view ptr) {
    nlohmann::json_pointer<std::string> ptrs = nlohmann::json_pointer<std::string>(std::string(ptr));
    return d_json.contains(ptrs) ? (d_json.erase(ptrs.to_string()), true) : false;
}

bool JsonStorage::has(std::string_view key) const {
    return d_json.contains(key);
}

bool JsonStorage::has_ptr(std::string_view ptr) const {
    return d_json.contains(nlohmann::json_pointer<std::string>(std::string(ptr)));
}

nlohmann::ordered_json& JsonStorage::get() {
    return d_json;
}

std::vector<std::string> JsonStorage::keys() const {
    std::vector<std::string> keys;
    keys.reserve(d_json.size());
    
    for (const auto& [key, _] : d_json.items())
        keys.emplace_back(key);
    return keys;
}

void JsonStorage::save() const {
    std::filesystem::create_directories(d_path.parent_path());
    if (std::ofstream file{d_path, std::ios::trunc})
        file << d_json.dump(4);
}
