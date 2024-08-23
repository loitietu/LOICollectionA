#include <memory>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "JsonUtils.h"
#include "I18nUtils.h"

namespace I18nUtils {
    namespace {
        using nlohmann::ordered_json;
        std::unique_ptr<JsonUtils> json;
    }

    void load(std::filesystem::path path) {
        json = std::make_unique<JsonUtils>(path);
    }

    std::string tr(const std::string& local, const std::string& key) {
        if (json->has(local)) {
            ordered_json localJson = json->toJson(local);
            if (localJson.contains(key)) {
                return localJson[key];
            }
        }
        return key;
    }

    std::vector<std::string> keys() {
        return json->keys();
    }
}