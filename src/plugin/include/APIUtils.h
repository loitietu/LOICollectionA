#pragma once

#include <string>
#include <functional>

#include "base/Macro.h"

class Player;

namespace LOICollection::LOICollectionAPI {
    LOICOLLECTION_A_API   void initialization();
    LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&)> callback);
    LOICOLLECTION_A_API   void registerVariable(const std::string& name, std::function<std::string(Player&, std::string)> callback);

    LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player);
    LOICOLLECTION_A_NDAPI std::string getValueForVariable(const std::string& name, Player& player, const std::string& parameter);

    LOICOLLECTION_A_NDAPI std::string tryGetGrammarResult(const std::string& contentString);

    LOICOLLECTION_A_API   std::string& translateString(std::string& contentString, Player& player);
    LOICOLLECTION_A_API   std::string& translateString(const std::string& contentString, Player& player);
}