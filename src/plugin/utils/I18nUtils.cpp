#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "I18nUtils.h"

namespace I18nUtils {
    std::string defaultLocal{};
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;

    void load(const std::filesystem::path& path) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;

            nlohmann::ordered_json dataJson;
            std::ifstream(entry.path()) >> dataJson;
            for (const auto& [key, value] : dataJson.items())
                data[entry.path().stem().string()][key] = value;
        }
    }

    void setDefaultLocal(const std::string& local) {
        defaultLocal = local;
    }

    std::string tr(std::string local, std::string key) {
        if (local.empty()) local = defaultLocal;
        if (data.find(local) != data.end() && data[local].find(key) != data[local].end())
            return data[local][key];
        return key;
    }

    std::vector<std::string> keys() {
        std::vector<std::string> keyl;
        for (const auto& [entry, _] : data)
            keyl.push_back(entry);
        return keyl;
    }
}
