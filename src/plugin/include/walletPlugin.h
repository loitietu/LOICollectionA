#ifndef LOICOLLECTION_A_WALLETPLUGIN_H
#define LOICOLLECTION_A_WALLETPLUGIN_H

#include <map>
#include <string>
#include <variant>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::wallet {
    namespace MainGui {
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void transfer(Player& player);
        LOICOLLECTION_A_API void offlineTransfer(Player& player);
        LOICOLLECTION_A_API void wealth(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API void registery(std::map<std::string, std::variant<std::string, double>>& options);
}

#endif