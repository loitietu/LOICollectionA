#pragma once

#include <memory>
#include <string>
#include <functional>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::LOICollectionAPI {
    class APIUtils {
    public:
        LOICOLLECTION_A_NDAPI static APIUtils& getInstance();

        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>()> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(Player&)> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(Player&, const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args);

        LOICOLLECTION_A_NDAPI ll::Expected<frontend::TypedValue> getValueForVariable(const std::string& name);
        LOICOLLECTION_A_NDAPI ll::Expected<frontend::TypedValue> getValueForVariable(const std::string& name, Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<frontend::TypedValue> getValueForVariable(const std::string& name, const frontend::CallbackTypeValues& parameter);
        LOICOLLECTION_A_NDAPI ll::Expected<frontend::TypedValue> getValueForVariable(const std::string& name, Player& player, const frontend::CallbackTypeValues& parameter);

        LOICOLLECTION_A_API   std::string translate(const std::string& str, Player& player);
        LOICOLLECTION_A_API   std::string translate(const std::string& str);

    private:
        APIUtils();
        ~APIUtils();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}