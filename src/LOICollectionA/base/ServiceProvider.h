#pragma once

#include <memory>

#include "LOICollectionA/base/ServiceContainer.h"

class ServiceProvider {
public:
    static ServiceProvider& getInstance() {
        static ServiceProvider instance;
        return instance;
    }

    [[nodiscard]] std::shared_ptr<ServiceContainer> getContainer() const {
        return mContainer;
    }

    template <typename TService>
    std::shared_ptr<TService> getService(const std::string& name = "") {
        return getContainer()->getService<TService>(name);
    }
    
    template <typename TService>
    std::vector<std::string> getServiceNames() {
        return getContainer()->getServiceNames<TService>();
    }
    
    template <typename TService>
    void registerInstance(std::shared_ptr<TService> instance, const std::string& name = "") {
        getContainer()->registerInstance<TService>(instance, name);
    }
    
    template <typename TService>
    bool isRegistered(const std::string& name = "") {
        return getContainer()->isRegistered<TService>(name);
    }

private:
    std::shared_ptr<ServiceContainer> mContainer;

    ServiceProvider() : mContainer(std::make_shared<ServiceContainer>()) {}
    ~ServiceProvider() = default;
};
