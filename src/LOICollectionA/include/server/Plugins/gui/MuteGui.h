#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class MutePlugin;

    class MuteGui {
    private:
        MutePlugin& mParent;

    public:
        MuteGui(MutePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void info(Player& player, const std::string& id);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}