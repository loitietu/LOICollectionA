#ifndef LOICOLLECTION_A_BLACKLIST_H
#define LOICOLLECTION_A_BLACKLIST_H

#include <string>

#include "base/Macro.h"

class Player;

enum class BlacklistType {
    ip,
    uuid
};

namespace LOICollection::Plugins::blacklist {
    namespace MainGui {
        LOICOLLECTION_A_API void info(Player& player, std::string target);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void addBlacklist(Player& player, std::string cause, int time, BlacklistType type);
    LOICOLLECTION_A_API   void delBlacklist(std::string target);
    
    LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif