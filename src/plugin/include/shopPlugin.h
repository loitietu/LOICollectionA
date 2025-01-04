#ifndef LOICOLLECTION_A_SHOPPLUGIN_H
#define LOICOLLECTION_A_SHOPPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "base/Macro.h"

class Player;

enum class ShopType {
    buy,
    sell
};

namespace LOICollection::Plugins::shop {
    namespace MainGui {
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, std::string uiName, ShopType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, std::string uiName, ShopType type);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, std::string uiName);
        LOICOLLECTION_A_API void editAwardContent(Player& player, std::string uiName);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void menu(Player& player, nlohmann::ordered_json& data, ShopType type);
        LOICOLLECTION_A_API void commodity(Player& player, nlohmann::ordered_json& data, nlohmann::ordered_json original, ShopType type);
        LOICOLLECTION_A_API void title(Player& player, nlohmann::ordered_json& data, nlohmann::ordered_json original, ShopType type);
        LOICOLLECTION_A_API void open(Player& player, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number);

    LOICOLLECTION_A_API   void registery(void* database);
}

#endif