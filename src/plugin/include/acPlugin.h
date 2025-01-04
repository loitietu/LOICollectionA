#ifndef LOICOLLECTION_A_ACPLUGIN_H
#define LOICOLLECTION_A_ACPLUGIN_H

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::announcement {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_NDAPI bool isClose(Player& player);

    LOICOLLECTION_A_API   void registery(void* database, void* config);
    LOICOLLECTION_A_API   void unregistery();
}

#endif