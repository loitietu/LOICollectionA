#pragma once

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class PvpPlugin;
    
    class PvpGui {
    private:
        PvpPlugin& mParent;

    public:
        PvpGui(PvpPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };    
}
