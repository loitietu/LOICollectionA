#pragma once

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class LanguagePlugin;
    
    class LanguageGui {
    private:
        LanguagePlugin& mParent;

    public:
        LanguageGui(LanguagePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player);
    };
}