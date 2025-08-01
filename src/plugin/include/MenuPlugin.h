#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "base/Macro.h"

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
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& uiName, MenuType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& uiName, MenuType type);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& uiName);
        LOICOLLECTION_A_API void editAwardCommand(Player& player, const std::string& uiName);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& uiName);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void custom(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void simple(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void modal(Player& player, nlohmann::ordered_json& data);
        LOICOLLECTION_A_API void open(Player& player, std::string uiName);
    }

    LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);
    LOICOLLECTION_A_API   void handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database);
    LOICOLLECTION_A_API   void unregistery();
}