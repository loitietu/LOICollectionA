#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/TpaType.h"

class Player;

namespace LOICollection::server::Plugins {
    class TpaPlugin;

    class TpaGui {
    private:
        TpaPlugin& mParent;

    public:
        TpaGui(TpaPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void generic(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void tpa(Player& player, Player& target, TpaType type);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void open(Player& player);
    };
}
