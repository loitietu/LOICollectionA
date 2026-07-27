#pragma once

#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/include/ModulePriority.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::modules {
    class ModuleBase;
    class ModManager {
    public:
        ModManager(const ModManager&) = delete;
        ModManager& operator=(const ModManager&) = delete;

        ModManager(ModManager&&) = delete;
        ModManager& operator=(ModManager&&) = delete;

        LOICOLLECTION_A_NDAPI static ModManager& getInstance();

        LOICOLLECTION_A_API   void registry(std::shared_ptr<ModuleBase> modules, const std::string& name, ModulePriority priority = ModulePriority::Normal);
        LOICOLLECTION_A_API   void unregistry(const std::string& name);

        LOICOLLECTION_A_NDAPI std::shared_ptr<ModuleBase> getModule(const std::string& name) const;

        LOICOLLECTION_A_NDAPI std::vector<std::string> mods() const;

    private:
        ModManager();
        ~ModManager();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    template <typename Derived>
    class AutoRegister {
    private:
        static inline bool registered = []() -> bool {
            std::shared_ptr<Derived> modules = Derived::getShared();
            ModManager::getInstance().registry(modules, modules->getName(), modules->getPriority());

            return true;
        }();

    protected:
        AutoRegister() {
            static_cast<void>(registered);
        }
    };
}
