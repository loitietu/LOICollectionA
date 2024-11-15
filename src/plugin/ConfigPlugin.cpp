#include <string>

#include <ll/api/Mod/Manifest.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/reflection/Serialization.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "data/JsonStorage.h"

#include "LOICollectionA.h"
#include "ll/api/utils/StringUtils.h"

#include "ConfigPlugin.h"

namespace Config {
    std::string getVersion() {
        return LOICollection::A::getInstance().getSelf().getManifest().version->to_string();
    }

    void mergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target) {
        for (auto it = source.begin(); it != source.end(); ++it) {
            if (!target.contains(it.key())) {
                target[it.key()] = it.value();
                continue;
            }
            if (it.value().is_object() && target[it.key()].is_object())
                mergeJson(it.value(), target[it.key()]);
            else if (it->type() != target[it.key()].type()) target[it.key()] = it.value();
        }
    }

    void SynchronousPluginConfigVersion(void* config_ptr) {
        C_Config* config = static_cast<C_Config*>(config_ptr);
        config->version = std::stoi(ll::string_utils::replaceAll(
            getVersion(), ".", ""
        ));
    }

    void SynchronousPluginConfigType(void* config_ptr, const std::string& path) {
        JsonStorage mConfigObject(path);
        auto patch = ll::reflection::serialize<nlohmann::ordered_json>(*static_cast<C_Config*>(config_ptr));
        nlohmann::ordered_json mPatchJson = nlohmann::ordered_json::parse(patch->dump());
        nlohmann::ordered_json mConfigJson = mConfigObject.toJson();
        mergeJson(mPatchJson, mConfigJson);
        mConfigObject.write(mConfigJson);
        mConfigObject.save();
    }
}