#pragma once

#include <string>
#include <vector>

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/include/server/Plugins/form/ScoreData.h"

namespace LOICollection::server::Plugins {
    struct ShopItemData {
        std::string type;
        std::string title;
        std::string introduce;
        std::string number;
        std::string id;
        std::string nbt;
        std::string confirmButton;
        std::string cancelButton;
        int time = 0;
        std::vector<ScoreRequirement> scores;
    };

    struct ShopData {
        std::string id;
        std::string type;
        std::string title;
        std::string content;
        std::string exitCommand;
        std::string scoreCommand;
        std::string titleCommand;
        std::string itemCommand;
        std::vector<ShopItemData> items;
    };

    LOICOLLECTION_A_NDAPI ShopData hydrateShopData(const frontend::ObjectRef& obj);
    LOICOLLECTION_A_NDAPI ShopItemData hydrateShopItem(const frontend::ObjectRef& obj);
    LOICOLLECTION_A_NDAPI frontend::ObjectRef makeShopItemDataObject(const ShopItemData& item);
}
