#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class NoticePlugin;

    class NoticeGui {
    private:
        NoticePlugin& mParent;

    public:
        NoticeGui(NoticePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void content(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentAdd(Player& player);
        LOICOLLECTION_A_API void contentRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentRemove(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void notice(Player& player);
        LOICOLLECTION_A_API void notice(Player& player, const std::string& id);
        LOICOLLECTION_A_API void open(Player& player);
    };    
}
