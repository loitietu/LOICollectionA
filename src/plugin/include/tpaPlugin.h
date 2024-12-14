#ifndef LOICOLLECTION_A_TPAPLUGIN_H
#define LOICOLLECTION_A_TPAPLUGIN_H

#include "ExportLib.h"

class Player;

enum class TpaType {
    tpa,
    tphere
};

namespace LOICollection::Plugins::tpa {
    namespace MainGui {
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void tpa(Player& player, Player& target, TpaType type);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_NDAPI bool isInvite(Player& player);
    
    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}

#endif