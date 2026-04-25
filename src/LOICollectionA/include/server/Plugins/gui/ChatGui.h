#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class ChatPlugin;

    class ChatGui {
    private:
        ChatPlugin& mParent;

    public:
        ChatGui(ChatPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void contentAdd(Player& player, Player& target);
        LOICOLLECTION_A_API void contentRemove(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void title(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}