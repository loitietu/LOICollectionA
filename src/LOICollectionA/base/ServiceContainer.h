#pragma once

#include <any>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <typeindex>
#include <unordered_map>

class ServiceContainer {
private:
    std::unordered_map<std::type_index, std::unordered_map<std::string, std::any>> mInstances;
    std::mutex mMutex;
    
public:
    template <typename TService>
    void registerInstance(std::shared_ptr<TService> instance, const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        mInstances[std::type_index(typeid(TService))][name] = instance;
    }
    
    template <typename TService>
    std::shared_ptr<TService> getService(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto it = mInstances.find(type);
        if (it != mInstances.end()) {
            auto instanceIt = it->second.find(name);
            if (instanceIt != it->second.end())
                return std::any_cast<std::shared_ptr<TService>>(instanceIt->second);
        }
        
        return nullptr;
    }
    
    template <typename TService>
    std::vector<std::string> getServiceNames() {
        std::lock_guard<std::mutex> lock(mMutex);
        
        std::vector<std::string> names;
        
        auto type = std::type_index(typeid(TService));
        
        auto it = mInstances.find(type);
        if (it != mInstances.end()) {
            for (const auto& pair : it->second)
                names.push_back(pair.first);
        }
        
        return names;
    }
    
    template <typename TService>
    bool isRegistered(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto it = mInstances.find(type);
        if (it != mInstances.end())
            return it->second.find(name) != it->second.end();
        
        return false;
    }
    
    template <typename TService>
    void removeService(const std::string& name = "") {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto type = std::type_index(typeid(TService));
        
        auto it = mInstances.find(type);
        if (it != mInstances.end()) {
            it->second.erase(name);
            if (it->second.empty())
                mInstances.erase(type);
        }
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mMutex);

        mInstances.clear();
    }
};
