#ifndef LOICOLLECTION_A_CDK_H
#define LOICOLLECTION_A_CDK_H

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

    LOICOLLECTION_A_API void cdkConvert(Player& player, std::string convertString);

    LOICOLLECTION_A_API void registery(void* database);
}

#endif