#pragma once

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class PvpPlugin;
    
    class PvpGui {
    private:
        PvpPlugin& mParent;

    public:
        PvpGui(PvpPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player);
    };    
}
