#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>

#include "base/Macro.h"

class Player;

enum class TpaType {
    tpa,
    tphere
};

namespace LOICollection::Plugins::tpa {
    namespace MainGui {
        LOICOLLECTION_A_API void generic(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void tpa(Player& player, Player& target, TpaType type);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
    LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

    LOICOLLECTION_A_NDAPI bool isInvite(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();
    
    LOICOLLECTION_A_API   void registery(void* database, void* setting, std::map<std::string, std::variant<std::string, int>>& options);
    LOICOLLECTION_A_API   void unregistery();
}