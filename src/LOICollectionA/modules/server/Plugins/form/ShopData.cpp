#include <string>
#include <vector>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/include/server/Plugins/form/ScoreData.h"
#include "LOICollectionA/include/server/Plugins/form/ShopData.h"

using namespace LOICollection::frontend;

namespace LOICollection::server::Plugins {
    namespace {
        std::string shopReadString(const ObjectRef& obj, const std::string& field, const std::string& def = "") {
            auto it = obj->fields.find(field);
            if (it == obj->fields.end())
                return def;
            return std::holds_alternative<std::string>(it->second) ? std::get<std::string>(it->second) : def;
        }

        int shopReadInt(const ObjectRef& obj, const std::string& field, int def = 0) {
            auto it = obj->fields.find(field);
            if (it == obj->fields.end())
                return def;
            if (std::holds_alternative<int>(it->second))
                return std::get<int>(it->second);
            if (std::holds_alternative<float>(it->second))
                return static_cast<int>(std::get<float>(it->second));
            return def;
        }
    }

    ShopData hydrateShopData(const frontend::ObjectRef& obj) {
        ShopData data;
        data.id = shopReadString(obj, "id");
        data.type = shopReadString(obj, "type");
        data.title = shopReadString(obj, "title");
        data.content = shopReadString(obj, "content");
        data.exitCommand = shopReadString(obj, "exitCommand");
        data.scoreCommand = shopReadString(obj, "scoreCommand");
        data.titleCommand = shopReadString(obj, "titleCommand");
        data.itemCommand = shopReadString(obj, "itemCommand");

        auto it = obj->fields.find("items");
        if (it != obj->fields.end() && std::holds_alternative<ArrayRef>(it->second)) {
            for (const auto& element : std::get<ArrayRef>(it->second)->elements) {
                if (std::holds_alternative<ObjectRef>(element) && std::get<ObjectRef>(element)->className == "ShopItemData")
                    data.items.push_back(hydrateShopItem(std::get<ObjectRef>(element)));
            }
        }

        return data;
    }

    ShopItemData hydrateShopItem(const frontend::ObjectRef& obj) {
        ShopItemData item;
        item.type = shopReadString(obj, "type");
        item.title = shopReadString(obj, "title");
        item.introduce = shopReadString(obj, "introduce");
        item.number = shopReadString(obj, "number");
        item.id = shopReadString(obj, "id");
        item.nbt = shopReadString(obj, "nbt");
        item.confirmButton = shopReadString(obj, "confirmButton");
        item.cancelButton = shopReadString(obj, "cancelButton");
        item.time = shopReadInt(obj, "time");
        item.scores = readScores(obj);
        return item;
    }

    frontend::ObjectRef makeShopItemDataObject(const ShopItemData& item) {
        auto obj = std::make_shared<Object>();
        obj->className = "ShopItemData";
        obj->classIndex = -1;
        obj->fields["type"] = item.type;
        obj->fields["title"] = item.title;
        obj->fields["introduce"] = item.introduce;
        obj->fields["number"] = item.number;
        obj->fields["id"] = item.id;
        obj->fields["nbt"] = item.nbt;
        obj->fields["confirmButton"] = item.confirmButton;
        obj->fields["cancelButton"] = item.cancelButton;
        obj->fields["time"] = item.time;
        obj->fields["scores"] = makeScoreArray(item.scores);
        return obj;
    }

    void registerShopDataClasses(const std::string&) {
        ClassCall& classes = ClassCall::getInstance();

        classes.registerClass("ShopData", {
            "id", "type", "title", "content",
            "exitCommand", "scoreCommand", "titleCommand", "itemCommand", "items"
        });
        classes.registerField("ShopData", "id", std::string(""));
        classes.registerField("ShopData", "type", std::string(""));
        classes.registerField("ShopData", "title", std::string(""));
        classes.registerField("ShopData", "content", std::string(""));
        classes.registerField("ShopData", "exitCommand", std::string(""));
        classes.registerField("ShopData", "scoreCommand", std::string(""));
        classes.registerField("ShopData", "titleCommand", std::string(""));
        classes.registerField("ShopData", "itemCommand", std::string(""));
        classes.registerField("ShopData", "items", std::make_shared<ArrayValue>());

        classes.registerClass("ShopItemData", {
            "type", "title", "introduce", "number", "id", "nbt",
            "confirmButton", "cancelButton", "time", "scores"
        });
        classes.registerField("ShopItemData", "type", std::string(""));
        classes.registerField("ShopItemData", "title", std::string(""));
        classes.registerField("ShopItemData", "introduce", std::string(""));
        classes.registerField("ShopItemData", "number", std::string(""));
        classes.registerField("ShopItemData", "id", std::string(""));
        classes.registerField("ShopItemData", "nbt", std::string(""));
        classes.registerField("ShopItemData", "confirmButton", std::string(""));
        classes.registerField("ShopItemData", "cancelButton", std::string(""));
        classes.registerField("ShopItemData", "time", 0);
        classes.registerField("ShopItemData", "scores", std::make_shared<ArrayValue>());
    }
}

REGISTER_CALLBACK(ShopData, LOICollection::server::Plugins::registerShopDataClasses)
