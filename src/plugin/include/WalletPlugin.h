#pragma once

#include <map>
#include <string>
#include <variant>

#include "base/Macro.h"

class Player;

namespace mce {
    class UUID;
}

enum class TransferType {
    online,
    offline
};

namespace LOICollection::Plugins::wallet {
    namespace MainGui {
        LOICOLLECTION_A_API void content(Player& player, mce::UUID target, TransferType type);
        LOICOLLECTION_A_API void transfer(Player& player, TransferType type);
        LOICOLLECTION_A_API void wealth(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    }

    LOICOLLECTION_A_API   void transfer(Player& player, mce::UUID target, int score, TransferType type, bool isReduce = true);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* setting, std::map<std::string, std::variant<std::string, double>>& options);
    LOICOLLECTION_A_API   void unregistery();
}