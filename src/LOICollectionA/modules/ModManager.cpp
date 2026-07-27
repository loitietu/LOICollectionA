#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModulePriority.h"

#include "LOICollectionA/include/ModManager.h"

namespace LOICollection::modules {
    struct Module {
        std::string name;

        ModulePriority priority { ModulePriority::Normal };

        std::shared_ptr<ModuleBase> registry;
    };

    struct ModManager::Impl {
        std::vector<Module> mRegistries;
    };

    ModManager::ModManager() : mImpl(std::make_unique<Impl>()) {}
    ModManager::~ModManager() = default;

    ModManager& ModManager::getInstance() {
        static ModManager instance;
        return instance;
    }

    void ModManager::registry(std::shared_ptr<ModuleBase> modules, const std::string& name, ModulePriority priority) {
        Module mModule;
        mModule.name = name;
        mModule.priority = priority;
        mModule.registry = std::move(modules);

        this->mImpl->mRegistries.emplace_back(std::move(mModule));

        std::sort(this->mImpl->mRegistries.begin(), this->mImpl->mRegistries.end(), [](const Module& a, const Module& b) -> bool {
            return a.priority < b.priority;
        });
    }

    void ModManager::unregistry(const std::string& name) {
        auto it = std::find_if(this->mImpl->mRegistries.begin(), this->mImpl->mRegistries.end(), [name](const Module& a) -> bool {
            return a.name == name;
        });

        if (it != this->mImpl->mRegistries.end())
            this->mImpl->mRegistries.erase(it);

        std::sort(this->mImpl->mRegistries.begin(), this->mImpl->mRegistries.end(), [](const Module& a, const Module& b) -> bool {
            return a.priority < b.priority;
        });
    }

    std::shared_ptr<ModuleBase> ModManager::getModule(const std::string& name) const {
        auto it = std::find_if(this->mImpl->mRegistries.begin(), this->mImpl->mRegistries.end(), [name](const Module& a) -> bool {
            return a.name == name;
        });

        if (it == this->mImpl->mRegistries.end())
            return nullptr;

        return it->registry;
    }

    std::vector<std::string> ModManager::mods() const {
        std::vector<std::string> mods;
        mods.reserve(this->mImpl->mRegistries.size());

        for (auto& it : this->mImpl->mRegistries)
            mods.push_back(it.name);

        return mods;
    }
}
