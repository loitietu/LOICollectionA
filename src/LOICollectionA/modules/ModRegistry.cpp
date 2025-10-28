#include <memory>
#include <string>

#include "LOICollectionA/include/ModRegistry.h"

namespace LOICollection::modules {
    struct ModRegistry::Impl {
        std::string name;

        CallBackFunc onLoaded;
        CallBackFunc onRegistry;
        CallBackFunc onUnregistry;
    };

    ModRegistry::ModRegistry(const std::string& name) : mImpl(std::make_unique<Impl>()) {
        this->mImpl->name = name;
    }

    ModRegistry::~ModRegistry() = default;

    std::string ModRegistry::getName() const noexcept {
        return this->mImpl->name;
    }

    bool ModRegistry::onLoad() {
        if (!this->isLoaded())
            return false;

        bool result = this->mImpl->onLoaded();
        return result;
    }

    bool ModRegistry::onRegistry() {
        if (!this->isRegistered())
            return false;

        bool result = this->mImpl->onRegistry();
        return result;
    }

    bool ModRegistry::onUnregistry() {
        if (!this->isUnregistered())
            return false;

        bool result = this->mImpl->onUnregistry();
        return result;
    }

    void ModRegistry::onLoad(CallBackFunc func) {
        this->mImpl->onLoaded = std::move(func);
    }

    void ModRegistry::onRegistry(CallBackFunc func) {
        this->mImpl->onRegistry = std::move(func);
    }

    void ModRegistry::onUnregistry(CallBackFunc func) {
        this->mImpl->onUnregistry = std::move(func);
    }

    void ModRegistry::release() noexcept {
        this->mImpl.reset();
    }

    bool ModRegistry::isLoaded() const noexcept {
        return this->mImpl->onLoaded != nullptr;
    }

    bool ModRegistry::isRegistered() const noexcept {
        return this->mImpl->onRegistry != nullptr;
    }

    bool ModRegistry::isUnregistered() const noexcept {
        return this->mImpl->onUnregistry != nullptr;
    }
}