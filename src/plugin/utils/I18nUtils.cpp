#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "I18nUtils.h"

namespace I18nUtils {
    std::unordered_map<std::string, nlohmann::ordered_json> data;

    void load(const std::filesystem::path& path) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file() || entry.path().extension()!= ".json") continue;

            std::ifstream filestream(entry.path());
            nlohmann::ordered_json dataJson;
            filestream >> dataJson;
            filestream.close();
            
            data[entry.path().stem().string()] = dataJson;
        }
    }

    std::string tr(const std::string& local, const std::string& key) {
        if (auto it = data.find(local); it != data.end()) {
            const nlohmann::ordered_json& localJson = it->second;
            if (localJson.contains(key))
                return localJson[key];
        }
        return key;
    }

    std::vector<std::string> keys() {
        std::vector<std::string> keyl;
        for (const auto& [entry, _] : data)
            keyl.push_back(entry);
        return keyl;
    }
}
