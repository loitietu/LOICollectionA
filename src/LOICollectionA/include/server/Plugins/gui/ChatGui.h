#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class ChatPlugin;

    class ChatGui {
    private:
        ChatPlugin& mParent;

    public:
        ChatGui(ChatPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> contentAdd(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> contentRemove(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> add(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> remove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> title(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklistAdd(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklist(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> setting(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}