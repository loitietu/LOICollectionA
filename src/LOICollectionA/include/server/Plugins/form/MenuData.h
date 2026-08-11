#pragma once

#include <string>
#include <vector>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/include/server/Plugins/form/ScoreData.h"

namespace LOICollection::server::Plugins {
    struct MenuItemData {
        std::string type;
        std::string title;
        std::string id;
        std::vector<std::string> run;
        int permission = 0;
        std::vector<ScoreRequirement> scores;
    };

    struct MenuControlData {
        std::string type;
        std::string id;
        std::string title;
        std::string placeholder;
        std::string tooltip;
        frontend::TypedValue defaultValue;
        std::vector<std::string> options;
        int min = 0;
        int max = 100;
        int step = 1;
    };

    struct MenuData {
        std::string id;
        std::string type;
        std::string title;
        std::string content;
        int permission = 0;
        std::string exitCommand;
        std::string scoreCommand;
        std::string permissionCommand;
        std::vector<MenuItemData> items;
        std::vector<MenuControlData> controls;
        MenuItemData confirm;
        MenuItemData cancel;
        std::vector<std::string> run;
        std::string submit;
    };

    LOICOLLECTION_A_NDAPI MenuData hydrateMenuData(const frontend::ObjectRef& obj);
    LOICOLLECTION_A_NDAPI MenuItemData hydrateMenuItem(const frontend::ObjectRef& obj);
    LOICOLLECTION_A_NDAPI frontend::ObjectRef makeMenuItemDataObject(const MenuItemData& item);
    LOICOLLECTION_A_NDAPI frontend::ObjectRef makeMenuControlDataObject(const MenuControlData& control);
}
