#include <string>

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
    C_Config mConfig;

    std::string GetVersion() {
        return LOICollection::A::getInstance().getSelf().getManifest().version->to_string();
    }

    C_Config& GetBaseConfigContext() {
        return mConfig;
    }

    void SetBaseConfigContext(C_Config& config) {
        mConfig = config;
    }

    void InsertJson(int pos, nlohmann::ordered_json::iterator& source, nlohmann::ordered_json& target) {
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

    void MergeJson(nlohmann::ordered_json& source, nlohmann::ordered_json& target) {
        for (auto it = source.begin(); it != source.end(); ++it) {
            if (!target.contains(it.key())) {
                InsertJson((int)std::distance(source.begin(), it), it, target);
                continue;
            }
            if (it.value().is_object() && target[it.key()].is_object()) {
                MergeJson(it.value(), target[it.key()]);
                continue;
            }
            if (it->type() != target[it.key()].type()) target[it.key()] = it.value();
        }
    }

    void SynchronousPluginConfigVersion(C_Config& config) {
        config.version = std::stoi(ll::string_utils::replaceAll(
            GetVersion(), ".", ""
        ));
    }

    void SynchronousPluginConfigType(C_Config& config, const std::string& path) {
        JsonStorage mConfigObject(path);
        nlohmann::ordered_json mPatchJson = nlohmann::ordered_json::parse(
            ll::reflection::serialize<nlohmann::ordered_json>(config)->dump()
        );
        MergeJson(mPatchJson, mConfigObject.get());
        mConfigObject.save();
    }
}