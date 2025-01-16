#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "I18nUtils.h"

I18nUtils::I18nUtils(const std::string& path) {
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;
        nlohmann::ordered_json dataJson;
        std::ifstream(entry.path()) >> dataJson;

        const std::string& locale = entry.path().stem().string();
        for (const auto& [key, value] : dataJson.items())
            this->set(locale, key, value);
    }
}

std::string I18nUtils::get(const std::string& local, const std::string& key) {
    if (auto localIt = this->data.find(local); localIt != data.end()) {
        const std::unordered_map<std::string, std::string>& translations = localIt->second;
        if (auto keyIt = translations.find(key); keyIt != translations.end())
            return keyIt->second;
    }
        
    if (auto defaultIt = this->data.find(this->defaultLocal); defaultIt != data.end()) {
        const std::unordered_map<std::string, std::string>& translations = defaultIt->second;
        if (auto keyIt = translations.find(key); keyIt != translations.end())
            return keyIt->second;
    }
    return key;
}