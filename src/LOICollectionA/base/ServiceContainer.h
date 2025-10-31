#pragma once

#include <any>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <typeindex>
#include <functional>
#include <unordered_map>

class ServiceContainer {
private:
    std::unordered_map<std::type_index, std::unordered_map<std::string, std::any>> mSingletonInstances;
    std::unordered_map<std::type_index, std::unordered_map<std::string, std::function<std::any()>>> mServiceFactories;
    std::mutex mMutex;
    
public:
    template<typename TService, typename TImplementation = TService>
    void registerSingleton(const std::string& name = "") {
        registerSingleton<TService, TImplementation>([]() {
            return std::make_shared<TImplementation>();
        }, name);
    }
    
    template<typename TService, typename TImplementation = TService, typename TFactory>
    void registerSingleton(TFactory&& factory, const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        mServiceFactories[std::type_index(typeid(TService))][name] = 
            [factory = std::forward<TFactory>(factory)]() -> std::any {
                return factory();
            };
    }
    
    template<typename TService>
    void registerInstance(std::shared_ptr<TService> instance, const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        mSingletonInstances[std::type_index(typeid(TService))][name] = instance;
    }
    
    template<typename TService>
    std::shared_ptr<TService> getService(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto instancesIt = mSingletonInstances.find(type);
        if (instancesIt != mSingletonInstances.end()) {
            auto instanceIt = instancesIt->second.find(name);
            if (instanceIt != instancesIt->second.end())
                return std::any_cast<std::shared_ptr<TService>>(instanceIt->second);
        }
        
        auto factoriesIt = mServiceFactories.find(type);
        if (factoriesIt != mServiceFactories.end()) {
            auto factoryIt = factoriesIt->second.find(name);
            if (factoryIt != factoriesIt->second.end()) {
                auto instance = std::any_cast<std::shared_ptr<TService>>(factoryIt->second());
                mSingletonInstances[type][name] = instance;
                
                return instance;
            }
        }
        
        return nullptr;
    }
    
    template<typename TService>
    std::vector<std::string> getServiceNames() {
        std::lock_guard<std::mutex> lock(mMutex);
        
        std::vector<std::string> names;
        auto type = std::type_index(typeid(TService));
        
        auto instancesIt = mSingletonInstances.find(type);
        if (instancesIt != mSingletonInstances.end()) {
            for (const auto& pair : instancesIt->second)
                names.push_back(pair.first);
        }
        
        auto factoriesIt = mServiceFactories.find(type);
        if (factoriesIt != mServiceFactories.end()) {
            for (const auto& pair : factoriesIt->second) {
                if (std::find(names.begin(), names.end(), pair.first) == names.end())
                    names.push_back(pair.first);
            }
        }
        
        return names;
    }
    
    template<typename TService>
    bool isRegistered(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto instancesIt = mSingletonInstances.find(type);
        if (instancesIt != mSingletonInstances.end())
            return instancesIt->second.find(name) != instancesIt->second.end();
        
        auto factoriesIt = mServiceFactories.find(type);
        if (factoriesIt != mServiceFactories.end())
            return factoriesIt->second.find(name) != factoriesIt->second.end();
        
        return false;
    }
    
    template<typename TService>
    bool isInstantiated(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        auto instancesIt = mSingletonInstances.find(type);
        
        return instancesIt != mSingletonInstances.end() && 
               instancesIt->second.find(name) != instancesIt->second.end();
    }
    
    template<typename TService>
    void removeService(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto instancesIt = mSingletonInstances.find(type);
        if (instancesIt != mSingletonInstances.end()) {
            instancesIt->second.erase(name);
            if (instancesIt->second.empty())
                mSingletonInstances.erase(type);
        }
        
        auto factoriesIt = mServiceFactories.find(type);
        if (factoriesIt != mServiceFactories.end()) {
            factoriesIt->second.erase(name);
            if (factoriesIt->second.empty())
                mServiceFactories.erase(type);
        }
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mMutex);

        mSingletonInstances.clear();
        mServiceFactories.clear();
    }
};
