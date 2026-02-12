#pragma once

#include <memory>
#include <string>
#include <functional>

#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::LOICollectionAPI {
    class APIUtils {
    public:
        LOICOLLECTION_A_NDAPI static APIUtils& getInstance();

        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string()> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&)> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&, const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args);

        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, const frontend::CallbackTypeValues& parameter);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player, const frontend::CallbackTypeValues& parameter);

        LOICOLLECTION_A_API   std::string translate(const std::string& str, Player& player);
        LOICOLLECTION_A_API   std::string translate(const std::string& str);

    private:
        APIUtils();
        ~APIUtils();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}