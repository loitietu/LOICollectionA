#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/WalletType.h"

class Player;

namespace LOICollection::server::Plugins {
    class WalletPlugin;

    class WalletGui {
    private:
        WalletPlugin& mParent;

    public:
        WalletGui(WalletPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> content(Player& player, const std::string& target, WalletTransferType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> transfer(Player& player, WalletTransferType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> redenvelope(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}