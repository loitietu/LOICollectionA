#pragma once

#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::modules {
    enum class ModulePriority : int {
        Higest = 0,
        High = 1,
        Normal = 2,
        Low = 3,
        Lowest = 4
    };

    class ModRegistry;
    class ModManager {
    public:
        ModManager(const ModManager&) = delete;
        ModManager& operator=(const ModManager&) = delete;

        ModManager(ModManager&&) = delete;
        ModManager& operator=(ModManager&&) = delete;

        LOICOLLECTION_A_NDAPI static ModManager& getInstance();

        LOICOLLECTION_A_API   void registry(std::unique_ptr<ModRegistry> registry, ModulePriority priority = ModulePriority::Normal);
        LOICOLLECTION_A_API   void unregistry(const std::string& name);

        LOICOLLECTION_A_NDAPI ModRegistry* getRegistry(const std::string& name) const;

        LOICOLLECTION_A_NDAPI std::vector<std::string> mods() const;

    private:
        ModManager();
        ~ModManager();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
