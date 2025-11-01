#pragma once

#include <memory>
#include <string>
#include <functional>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::modules {
    class ModRegistry {
    public:
        using CallBackType = bool();
        using CallBackFunc = std::function<CallBackType>;

        LOICOLLECTION_A_API   explicit ModRegistry(const std::string& name);
        LOICOLLECTION_A_API   ~ModRegistry();

        LOICOLLECTION_A_NDAPI std::string getName() const noexcept;

        LOICOLLECTION_A_API   bool onLoad();
        LOICOLLECTION_A_API   bool onUnload();
        LOICOLLECTION_A_API   bool onRegistry();
        LOICOLLECTION_A_API   bool onUnregistry();

        LOICOLLECTION_A_API   void onLoad(CallBackFunc func);
        LOICOLLECTION_A_API   void onUnload(CallBackFunc func);
        LOICOLLECTION_A_API   void onRegistry(CallBackFunc func);
        LOICOLLECTION_A_API   void onUnregistry(CallBackFunc func);

        LOICOLLECTION_A_API   void release() noexcept;

        LOICOLLECTION_A_NDAPI bool isLoaded() const noexcept;
        LOICOLLECTION_A_NDAPI bool isUnloaded() const noexcept;
        LOICOLLECTION_A_NDAPI bool isRegistered() const noexcept;
        LOICOLLECTION_A_NDAPI bool isUnregistered() const noexcept;

    private:
        struct Impl;

        std::unique_ptr<Impl> mImpl;
    };
}
