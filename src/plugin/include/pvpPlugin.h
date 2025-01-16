#pragma once

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::pvp {
    namespace MainGui {
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void enable(Player& player, bool value);

    LOICOLLECTION_A_NDAPI bool isEnable(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}