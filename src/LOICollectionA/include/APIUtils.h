#pragma once

#include <memory>
#include <string>
#include <functional>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::LOICollectionAPI {
    class APIUtils {
    public:
        LOICOLLECTION_A_NDAPI static APIUtils& getInstance();

        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&)> callback);
        LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&, std::string)> callback);

        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player);
        LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player, const std::string& parameter);

        LOICOLLECTION_A_NDAPI std::string tryGetGrammarResult(const std::string& str);

        LOICOLLECTION_A_NDAPI std::string getVariableString(const std::string& str, Player& player);
        LOICOLLECTION_A_NDAPI std::string getGrammarString(const std::string& str);

        LOICOLLECTION_A_API   std::string translateString(const std::string& str, Player& player);

    private:
        APIUtils();
        ~APIUtils();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}