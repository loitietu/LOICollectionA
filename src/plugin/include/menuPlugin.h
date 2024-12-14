#ifndef LOICOLLECTION_A_MENUPLUGIN_H
#define LOICOLLECTION_A_MENUPLUGIN_H

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "ExportLib.h"

class Player;

enum class MenuType {
    Simple,
    Modal,
    Custom
};

namespace LOICollection::Plugins::menu {
    namespace MainGui {
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, std::string uiName, MenuType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, std::string uiName, MenuType type);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, std::string uiName);
        LOICOLLECTION_A_API void editAwardCommand(Player& player, std::string uiName);
        LOICOLLECTION_A_API void editAwardContent(Player& player, std::string uiName);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void custom(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void simple(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void modal(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void open(Player& player, std::string uiName);
    }

    LOICOLLECTION_A_NDAPI bool checkModifiedData(Player& player, nlohmann::ordered_json& data);

    LOICOLLECTION_A_API   void logicalExecution(Player& player, nlohmann::ordered_json& data, nlohmann::ordered_json original);

    LOICOLLECTION_A_API   void registery(void* database, std::string itemid);
    LOICOLLECTION_A_API   void unregistery();
}

#endif