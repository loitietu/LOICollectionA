#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class BlacklistPlugin;

    class BlacklistGui {
    private:
        BlacklistPlugin& mParent;

    public:
        BlacklistGui(BlacklistPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> info(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> content(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> add(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> remove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}