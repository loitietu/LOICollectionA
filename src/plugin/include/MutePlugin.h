#pragma once

#include <string>
#include <vector>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::mute {
    namespace MainGui {
        LOICOLLECTION_A_API void info(Player& player, const std::string& id);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void addMute(Player& player, const std::string& cause, int time);
    LOICOLLECTION_A_API   void delMute(Player& player);
    LOICOLLECTION_A_API   void delMute(const std::string& target);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getMutes();
    
    LOICOLLECTION_A_NDAPI bool isMute(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}