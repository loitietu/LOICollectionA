#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::modules {
    class ModRegistry;
    class ModManager final {
    public:
        static ModManager& getInstance() {
            static ModManager instance;
            return instance;
        }

        ModManager(const ModManager&) = delete;
        ModManager& operator=(const ModManager&) = delete;

        ModManager(ModManager&&) = delete;
        ModManager& operator=(ModManager&&) = delete;

        LOICOLLECTION_A_API   void registry(std::unique_ptr<ModRegistry> registry);
        LOICOLLECTION_A_API   void unregistry(const std::string& name);

        LOICOLLECTION_A_NDAPI ModRegistry* getRegistry(const std::string& name) const;

        LOICOLLECTION_A_NDAPI std::vector<std::string> mods() const;

    private:
        ModManager() = default;
        ~ModManager() = default;

        std::unordered_map<std::string, std::unique_ptr<ModRegistry>> mRegistries;
    };
}
