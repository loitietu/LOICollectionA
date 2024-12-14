#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "I18nUtils.h"

namespace I18nUtils {
    static std::string defaultLocal{};
    static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;

    void load(const std::filesystem::path& path) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            nlohmann::ordered_json dataJson;
            std::ifstream(entry.path()) >> dataJson;

            auto& localeData = data[entry.path().stem().string()];
            for (const auto& [key, value] : dataJson.items())
                localeData[key] = value;
        }
    }

    void setDefaultLocal(const std::string& local) {
        defaultLocal = local;
    }

    std::string tr(const std::string& local, const std::string& key) {
        if (auto localIt = data.find(local); localIt != data.end()) {
            const auto& translations = localIt->second;
            if (auto keyIt = translations.find(key); keyIt != translations.end())
                return keyIt->second;
        }
        
        if (auto defaultIt = data.find(defaultLocal); defaultIt != data.end()) {
            const auto& translations = defaultIt->second;
            if (auto keyIt = translations.find(key); keyIt != translations.end())
                return keyIt->second;
        }
        
        return key;
    }

    
    std::vector<std::string> keys() {
        std::vector<std::string> keyList;

        keyList.reserve(data.size());
        for (const auto& [locale, _] : data)
            keyList.push_back(locale);
        return keyList;
    }
}
