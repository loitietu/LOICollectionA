#pragma once

#include <string>

#include "base/Macro.h"

class Player;

enum class TransferType {
    online,
    offline
};

namespace LOICollection::Plugins::wallet {
    namespace MainGui {
        LOICOLLECTION_A_API void content(Player& player, const std::string& target, TransferType type);
        LOICOLLECTION_A_API void transfer(Player& player, TransferType type);
        LOICOLLECTION_A_API void redenvelope(Player& player);
        LOICOLLECTION_A_API void wealth(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void transfer(const std::string& target, int score);
    LOICOLLECTION_A_API   void redenvelope(Player& player, const std::string& key, int score, int count);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* setting);
    LOICOLLECTION_A_API   void unregistery();
}