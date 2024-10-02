#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonUtils.h"

#include "I18nUtils.h"

namespace I18nUtils {
    std::unique_ptr<JsonUtils> json;

    void load(std::filesystem::path path) {
        json = std::make_unique<JsonUtils>(path);
    }

    std::string tr(const std::string& local, const std::string& key) {
        if (json->has(local)) {
            nlohmann::ordered_json localJson = json->toJson(local);
            if (localJson.contains(key)) {
                return localJson[key];
            }
        }
        return key;
    }

    std::string getName(const std::string& local) {
        nlohmann::ordered_json localJson = json->toJson(local);
        if (localJson.contains("name")) {
            return localJson["name"];
        }
        return local;
    }

    std::string getLocalFromName(const std::string& name) {
        for (auto& local : json->keys()) {
            if (getName(local) == name) {
                return local;
            }
        }
        return "";
    }

    std::vector<std::string> keys() {
        std::vector<std::string> keyl;
        for (auto& local : json->keys()) {
            keyl.push_back(getName(local));
        }
        return keyl;
    }
}