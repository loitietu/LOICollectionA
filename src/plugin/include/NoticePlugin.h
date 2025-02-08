#pragma once

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::notice {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_NDAPI bool isClose(Player& player);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, void* config);
    LOICOLLECTION_A_API   void unregistery();
}