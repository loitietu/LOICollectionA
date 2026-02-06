#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <shared_mutex>

#include <nlohmann/json.hpp>

#include "LOICollectionA/data/JsonStorage.h"

JsonStorage::JsonStorage(std::filesystem::path path) : mPath(std::move(path)) {
    if (std::filesystem::exists(mPath)) {
        std::ifstream file(mPath, std::ios::binary);
        
        if (file.is_open())
            file >> this->mJson;

        return;
    }

    std::filesystem::create_directories(mPath.parent_path());

    this->mJson = nlohmann::ordered_json::object();

    this->save();
}
JsonStorage::~JsonStorage() = default;

void JsonStorage::write(const nlohmann::json& json) {
    std::unique_lock lock(this->mMutex);
    
    this->mJson = json;
}

void JsonStorage::remove(std::string_view key) {
    std::unique_lock lock(this->mMutex);

    this->mJson.erase(key);
}

void JsonStorage::remove_ptr(std::string_view ptr) {
    std::unique_lock lock(this->mMutex);

    nlohmann::json_pointer<std::string> ptrs((std::string(ptr)));
    if (!this->mJson.contains(ptrs)) 
        return;

    auto& parent = this->mJson.at(ptrs.parent_pointer());
    parent.erase(ptrs.back());
}

bool JsonStorage::has(std::string_view key) const {
    std::shared_lock lock(this->mMutex);
    
    return this->mJson.contains(key);
}

bool JsonStorage::has_ptr(std::string_view ptr) const {
    std::shared_lock lock(this->mMutex);

    nlohmann::json_pointer<std::string> ptrs((std::string(ptr)));
    return this->mJson.contains(ptrs);
}

nlohmann::ordered_json JsonStorage::get() const {
    return this->mJson;
}

std::vector<std::string> JsonStorage::keys() const {
    std::shared_lock lock(this->mMutex);

    std::vector<std::string> keys;
    keys.reserve(this->mJson.size());

    for (const auto& item : this->mJson.items())
        keys.emplace_back(item.key());

    return keys;
}

void JsonStorage::save() const {
    std::unique_lock lock(this->mMutex);

    std::filesystem::create_directories(this->mPath.parent_path());
    std::ofstream file(this->mPath, std::ios::binary | std::ios::trunc);

    if (file.is_open())
        file << this->mJson.dump(4);
}
