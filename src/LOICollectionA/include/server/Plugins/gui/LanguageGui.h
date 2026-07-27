#pragma once

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class LanguagePlugin;
    
    class LanguageGui {
    private:
        LanguagePlugin& mParent;

    public:
        LanguageGui(LanguagePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}