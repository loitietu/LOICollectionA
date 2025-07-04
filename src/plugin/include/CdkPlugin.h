#pragma once

#include <string>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::cdk {
    namespace MainGui {
        LOICOLLECTION_A_API void convert(Player& player);
        LOICOLLECTION_A_API void cdkNew(Player& player);
        LOICOLLECTION_A_API void cdkRemove(Player& player);
        LOICOLLECTION_A_API void cdkAwardScore(Player& player);
        LOICOLLECTION_A_API void cdkAwardItem(Player& player);
        LOICOLLECTION_A_API void cdkAwardTitle(Player& player);
        LOICOLLECTION_A_API void cdkAward(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void cdkConvert(Player& player, const std::string& convertString);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}