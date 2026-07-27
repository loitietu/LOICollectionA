#pragma once

#include <memory>
#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/include/ModulePriority.h"

namespace ll::io {
    class Logger;
}

namespace LOICollection::modules {
    template <typename T, typename T2 = void>
    ll::Expected<T2> defaultErrorHandler(const ll::Error& e) {
        if constexpr (requires {
            { T::getShared()->getLogger() } -> std::convertible_to<std::shared_ptr<ll::io::Logger>>;
        }) {
            e.log(*T::getShared()->getLogger());
        }

        return {};
    }

    class ModuleBase {
    public:
        ModuleBase() = default;
        virtual ~ModuleBase() = default;

        ModuleBase(ModuleBase const&) = delete;
        ModuleBase(ModuleBase&&) = delete;
        ModuleBase& operator=(ModuleBase const&) = delete;
        ModuleBase& operator=(ModuleBase&&) = delete;

    public:
        virtual std::string getName() = 0;
        virtual ModulePriority getPriority() = 0;

    public:
        virtual ll::Expected<bool> load() = 0;
        virtual ll::Expected<bool> unload() = 0;
        virtual ll::Expected<bool> registry() = 0;
        virtual ll::Expected<bool> unregistry() = 0;
    };
}
