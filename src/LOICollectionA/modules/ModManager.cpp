#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/include/ModRegistry.h"

#include "LOICollectionA/include/ModManager.h"

namespace LOICollection::modules {
    void ModManager::registry(std::unique_ptr<ModRegistry> registry) {
        this->mRegistries[registry->getName()] = std::move(registry);
    }

    void ModManager::unregistry(const std::string& name) {
        auto it = this->mRegistries.find(name);
        if (it == this->mRegistries.end()) 
            return;

        this->mRegistries.erase(it);
    }

    ModRegistry* ModManager::getRegistry(const std::string& name) const {
        auto it = this->mRegistries.find(name);
        if (it == this->mRegistries.end())
            return nullptr;

        return it->second.get();
    }

    std::vector<std::string> ModManager::mods() const {
        std::vector<std::string> mods;
        mods.reserve(this->mRegistries.size());

        for (auto& it : this->mRegistries)
            mods.push_back(it.first);
        
        return mods;
    }
}
