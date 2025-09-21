#pragma once

#include <string>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::notice {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void content(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentAdd(Player& player);
        LOICOLLECTION_A_API void contentRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentRemove(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void notice(Player& player);
        LOICOLLECTION_A_API void notice(Player& player, const std::string& id);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_NDAPI bool isClose(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, void* setting);
    LOICOLLECTION_A_API   void unregistery();
}