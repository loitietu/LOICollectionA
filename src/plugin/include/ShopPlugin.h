#pragma once

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
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& uiName, ShopType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& uiName, ShopType type);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& uiName);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& uiName);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void menu(Player& player, nlohmann::ordered_json& data, ShopType type);
        LOICOLLECTION_A_API void commodity(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type);
        LOICOLLECTION_A_API void title(Player& player, nlohmann::ordered_json& data, const nlohmann::ordered_json& original, ShopType type);
        LOICOLLECTION_A_API void open(Player& player, std::string uiName);
    }

    LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);

    LOICOLLECTION_A_NDAPI bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number);
    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}