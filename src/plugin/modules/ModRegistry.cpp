#include <memory>
#include <string>

#include "include/ModRegistry.h"

namespace LOICollection::modules {
    struct ModRegistry::Impl {
        std::string name;

        CallBackFunc onLoaded;
        CallBackFunc onRegistry;
        CallBackFunc onUnRegistry;
    };

    ModRegistry::ModRegistry(const std::string& name) : mImpl(std::make_unique<Impl>()) {
        mImpl->name = name;
    }

    ModRegistry::~ModRegistry() = default;

    std::string ModRegistry::getName() const noexcept {
        return mImpl->name;
    }

    bool ModRegistry::onLoad() {
        if (!isLoaded())
            return false;

        bool result = mImpl->onLoaded();
        return result;
    }

    bool ModRegistry::onRegistry() {
        if (!isRegistered())
            return false;

        bool result = mImpl->onRegistry();
        return result;
    }

    bool ModRegistry::onUnRegistry() {
        if (!isUnRegistered())
            return false;

        bool result = mImpl->onUnRegistry();
        return result;
    }

    void ModRegistry::onLoad(CallBackFunc func) {
        mImpl->onLoaded = func;
    }

    void ModRegistry::onRegistry(CallBackFunc func) {
        mImpl->onRegistry = func;
    }

    void ModRegistry::onUnRegistry(CallBackFunc func) {
        mImpl->onUnRegistry = func;
    }

    void ModRegistry::release() noexcept {
        mImpl.reset();
    }

    bool ModRegistry::isLoaded() const noexcept {
        return mImpl->onLoaded != nullptr;
    }

    bool ModRegistry::isRegistered() const noexcept {
        return mImpl->onRegistry != nullptr;
    }

    bool ModRegistry::isUnRegistered() const noexcept {
        return mImpl->onUnRegistry != nullptr;
    }
}