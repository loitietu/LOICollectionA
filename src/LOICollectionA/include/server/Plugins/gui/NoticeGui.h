#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class NoticePlugin;

    class NoticeGui {
    private:
        NoticePlugin& mParent;

    public:
        NoticeGui(NoticePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> setting(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> content(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> contentAdd(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> contentRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> contentRemove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> edit(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> notice(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> notice(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };    
}
