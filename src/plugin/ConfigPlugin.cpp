#include <string>
#include <vector>

#include <ll/api/Mod/Manifest.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/reflection/Serialization.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "data/JsonStorage.h"

#include "LOICollectionA.h"

#include "ConfigPlugin.h"

namespace Config {
    std::string getVersion() {
        return LOICollection::A::getInstance().getSelf().getManifest().version->to_string();
    }

    void insertJson(int pos, nlohmann::ordered_json::iterator& source, nlohmann::ordered_json& target) {
        nlohmann::ordered_json mInsertJson;
        for (auto it = target.begin(); it != target.end(); ++it) {
            if ((int)std::distance(target.begin(), it) == pos)
                mInsertJson[source.key()] = source.value();
            mInsertJson[it.key()] = it.value();
        }
        if (pos == (int)std::distance(target.begin(), target.end()))
            mInsertJson[source.key()] = source.value();
        target = mInsertJson;
    }

    void mergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target) {
        std::vector<std::string> mRemoveKeys;
        for (auto it = target.begin(); it != target.end(); ++it) {
            if (!source.contains(it.key()))
                mRemoveKeys.push_back(it.key());
        }
        for (const std::string& key : mRemoveKeys) target.erase(key);
        for (auto it = source.begin(); it != source.end(); ++it) {
            if (!target.contains(it.key())) {
                insertJson((int)std::distance(source.begin(), it), it, target);
                continue;
            }
            if (it.value().is_object() && target[it.key()].is_object()) {
                mergeJson(it.value(), target[it.key()]);
                continue;
            }
            if (it->type() != target[it.key()].type()) target[it.key()] = it.value();
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
        nlohmann::ordered_json mPatchJson = nlohmann::ordered_json::parse(
            ll::reflection::serialize<nlohmann::ordered_json>(*static_cast<C_Config*>(config_ptr))->dump()
        );
        nlohmann::ordered_json mConfigJson = mConfigObject.toJson();
        mergeJson(mPatchJson, mConfigJson);
        mConfigObject.write(mConfigJson);
        mConfigObject.save();
    }
}