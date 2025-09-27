#include <memory>
#include <string>
#include <vector>

#include "include/ModRegistry.h"

#include "include/ModManager.h"

namespace LOICollection::modules {
    void ModManager::registry(std::unique_ptr<ModRegistry> registry) {
        mRegistries[registry->getName()] = std::move(registry);
    }

    void ModManager::unregistry(const std::string& name) {
        auto it = mRegistries.find(name);
        if (it == mRegistries.end()) 
            return;

        mRegistries.erase(it);
    }

    ModRegistry* ModManager::getRegistry(const std::string& name) const {
        auto it = mRegistries.find(name);
        if (it == mRegistries.end())
            return nullptr;

        return it->second.get();
    }

    std::vector<std::string> ModManager::mods() const {
        std::vector<std::string> mods;
        mods.reserve(mRegistries.size());

        for (auto& it : mRegistries)
            mods.push_back(it.first);
        return mods;
    }
}
