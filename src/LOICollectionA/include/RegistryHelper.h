#pragma once

#include <memory>
#include <string>
#include <concepts>

#include "LOICollectionA/include/ModManager.h"
#include "LOICollectionA/include/ModRegistry.h"

namespace LOICollection::modules {
    template <typename T>
    concept Loadable = requires(T t) {
        { t.load() } -> std::same_as<bool>;
    };

    template <typename T>
    concept Unloadable = requires(T t) {
        { t.unload() } -> std::same_as<bool>;
    };

    template <typename T>
    concept Registryable = requires(T t) {
        { t.registry() } -> std::same_as<bool>;
    };

    template <typename T>
    concept Unregistryable = requires(T t) {
        { t.unregistry() } -> std::same_as<bool>;
    };

    template <typename C, typename B>
    void registry(const std::string& name, B& binder, ModulePriority priority) {
        std::unique_ptr<ModRegistry> registry = std::make_unique<ModRegistry>(name);

        registry->onLoad(std::bind(&C::load, &binder));
        registry->onUnload(std::bind(&C::unload, &binder));
        registry->onRegistry(std::bind(&C::registry, &binder));
        registry->onUnregistry(std::bind(&C::unregistry, &binder));

        ModManager::getInstance().registry(std::move(registry), priority);
    }
}

#define REGISTRY_HELPER(NAME, CLASS, BINDER, PRIORITY)                                                  \
const auto RegistryHelper = []() -> bool {                                                              \
    static_assert(LOICollection::modules::Loadable<CLASS>, #CLASS " must be loadable");                 \
    static_assert(LOICollection::modules::Unloadable<CLASS>, #CLASS " must be unloadable");             \
    static_assert(LOICollection::modules::Registryable<CLASS>, #CLASS " must be registryable");         \
    static_assert(LOICollection::modules::Unregistryable<CLASS>, #CLASS " must be unregistryable");     \
                                                                                                        \
    LOICollection::modules::registry<CLASS>(NAME, BINDER, PRIORITY);                                    \
    return true;                                                                                        \
}();
