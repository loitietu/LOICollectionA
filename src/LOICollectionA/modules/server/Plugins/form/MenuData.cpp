#include <string>
#include <vector>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/include/server/Plugins/form/ScoreData.h"
#include "LOICollectionA/include/server/Plugins/form/MenuData.h"

using namespace LOICollection::frontend;

namespace LOICollection::server::Plugins {
    namespace {
        std::string menuReadString(const ObjectRef& obj, const std::string& field, const std::string& def = "") {
            auto it = obj->fields.find(field);
            if (it == obj->fields.end())
                return def;

            return std::holds_alternative<std::string>(it->second) ? std::get<std::string>(it->second) : def;
        }

        int menuReadInt(const ObjectRef& obj, const std::string& field, int def = 0) {
            auto it = obj->fields.find(field);
            if (it == obj->fields.end())
                return def;
            if (std::holds_alternative<int>(it->second))
                return std::get<int>(it->second);
            if (std::holds_alternative<float>(it->second))
                return static_cast<int>(std::get<float>(it->second));
            return def;
        }

        TypedValue menuReadValue(const ObjectRef& obj, const std::string& field, const TypedValue& def = {}) {
            auto it = obj->fields.find(field);
            return it == obj->fields.end() ? def : it->second;
        }

        std::vector<std::string> menuReadStringArray(const ObjectRef& obj, const std::string& field) {
            std::vector<std::string> result;
            auto it = obj->fields.find(field);
            if (it == obj->fields.end() || !std::holds_alternative<ArrayRef>(it->second))
                return result;
            for (const auto& element : std::get<ArrayRef>(it->second)->elements) {
                if (std::holds_alternative<std::string>(element))
                    result.push_back(std::get<std::string>(element));
            }
            return result;
        }

        MenuControlData readMenuControl(const ObjectRef& obj) {
            MenuControlData control;
            control.type = menuReadString(obj, "type");
            control.id = menuReadString(obj, "id");
            control.title = menuReadString(obj, "title");
            control.placeholder = menuReadString(obj, "placeholder");
            control.tooltip = menuReadString(obj, "tooltip");
            control.defaultValue = menuReadValue(obj, "defaultValue");
            control.options = menuReadStringArray(obj, "options");
            control.min = menuReadInt(obj, "min", 0);
            control.max = menuReadInt(obj, "max", 100);
            control.step = menuReadInt(obj, "step", 1);
            return control;
        }
    }

    MenuData hydrateMenuData(const frontend::ObjectRef& obj) {
        MenuData data;
        data.id = menuReadString(obj, "id");
        data.type = menuReadString(obj, "type");
        data.title = menuReadString(obj, "title");
        data.content = menuReadString(obj, "content");
        data.permission = menuReadInt(obj, "permission");
        data.exitCommand = menuReadString(obj, "exitCommand");
        data.scoreCommand = menuReadString(obj, "scoreCommand");
        data.permissionCommand = menuReadString(obj, "permissionCommand");
        data.run = menuReadStringArray(obj, "run");
        data.submit = menuReadString(obj, "submit");

        if (auto it = obj->fields.find("items"); it != obj->fields.end() && std::holds_alternative<ArrayRef>(it->second)) {
            for (const auto& element : std::get<ArrayRef>(it->second)->elements) {
                if (std::holds_alternative<ObjectRef>(element) && std::get<ObjectRef>(element)->className == "MenuItemData")
                    data.items.push_back(hydrateMenuItem(std::get<ObjectRef>(element)));
            }
        }

        if (auto it = obj->fields.find("controls"); it != obj->fields.end() && std::holds_alternative<ArrayRef>(it->second)) {
            for (const auto& element : std::get<ArrayRef>(it->second)->elements) {
                if (std::holds_alternative<ObjectRef>(element) && std::get<ObjectRef>(element)->className == "MenuControlData")
                    data.controls.push_back(readMenuControl(std::get<ObjectRef>(element)));
            }
        }

        if (auto it = obj->fields.find("confirm"); it != obj->fields.end() && std::holds_alternative<ObjectRef>(it->second))
            data.confirm = hydrateMenuItem(std::get<ObjectRef>(it->second));
        if (auto it = obj->fields.find("cancel"); it != obj->fields.end() && std::holds_alternative<ObjectRef>(it->second))
            data.cancel = hydrateMenuItem(std::get<ObjectRef>(it->second));

        return data;
    }

    MenuItemData hydrateMenuItem(const frontend::ObjectRef& obj) {
        MenuItemData item;
        item.type = menuReadString(obj, "type");
        item.title = menuReadString(obj, "title");
        item.id = menuReadString(obj, "id");
        item.run = menuReadStringArray(obj, "run");
        item.permission = menuReadInt(obj, "permission");
        item.scores = readScores(obj);
        return item;
    }

    frontend::ObjectRef makeMenuItemDataObject(const MenuItemData& item) {
        auto obj = std::make_shared<Object>();
        obj->className = "MenuItemData";
        obj->classIndex = -1;
        obj->fields["type"] = item.type;
        obj->fields["title"] = item.title;
        obj->fields["id"] = item.id;

        auto run = std::make_shared<ArrayValue>();
        for (const auto& command : item.run)
            run->elements.emplace_back(command);
        obj->fields["run"] = run;

        obj->fields["permission"] = item.permission;
        obj->fields["scores"] = makeScoreArray(item.scores);
        return obj;
    }

    frontend::ObjectRef makeMenuControlDataObject(const MenuControlData& control) {
        auto obj = std::make_shared<Object>();
        obj->className = "MenuControlData";
        obj->classIndex = -1;
        obj->fields["type"] = control.type;
        obj->fields["id"] = control.id;
        obj->fields["title"] = control.title;
        obj->fields["placeholder"] = control.placeholder;
        obj->fields["tooltip"] = control.tooltip;
        obj->fields["defaultValue"] = control.defaultValue;

        auto options = std::make_shared<ArrayValue>();
        for (const auto& option : control.options)
            options->elements.emplace_back(option);
        obj->fields["options"] = options;

        obj->fields["min"] = control.min;
        obj->fields["max"] = control.max;
        obj->fields["step"] = control.step;

        return obj;
    }

    void registerMenuDataClasses(const std::string&) {
        ClassCall& classes = ClassCall::getInstance();

        classes.registerClass("MenuData", {
            "id", "type", "title", "content", "permission",
            "exitCommand", "scoreCommand", "permissionCommand",
            "items", "controls", "confirm", "cancel", "run", "submit"
        });
        classes.registerField("MenuData", "id", std::string(""));
        classes.registerField("MenuData", "type", std::string(""));
        classes.registerField("MenuData", "title", std::string(""));
        classes.registerField("MenuData", "content", std::string(""));
        classes.registerField("MenuData", "permission", 0);
        classes.registerField("MenuData", "exitCommand", std::string(""));
        classes.registerField("MenuData", "scoreCommand", std::string(""));
        classes.registerField("MenuData", "permissionCommand", std::string(""));
        classes.registerField("MenuData", "items", std::make_shared<ArrayValue>());
        classes.registerField("MenuData", "controls", std::make_shared<ArrayValue>());
        classes.registerField("MenuData", "confirm", TypedValue{});
        classes.registerField("MenuData", "cancel", TypedValue{});
        classes.registerField("MenuData", "run", std::make_shared<ArrayValue>());
        classes.registerField("MenuData", "submit", std::string(""));

        classes.registerClass("MenuItemData", { "type", "title", "id", "run", "permission", "scores" });
        classes.registerField("MenuItemData", "type", std::string(""));
        classes.registerField("MenuItemData", "title", std::string(""));
        classes.registerField("MenuItemData", "id", std::string(""));
        classes.registerField("MenuItemData", "run", std::make_shared<ArrayValue>());
        classes.registerField("MenuItemData", "permission", 0);
        classes.registerField("MenuItemData", "scores", std::make_shared<ArrayValue>());

        classes.registerClass("MenuControlData", {
            "type", "id", "title", "placeholder", "defaultValue",
            "options", "min", "max", "step", "tooltip"
        });
        classes.registerField("MenuControlData", "type", std::string(""));
        classes.registerField("MenuControlData", "id", std::string(""));
        classes.registerField("MenuControlData", "title", std::string(""));
        classes.registerField("MenuControlData", "placeholder", std::string(""));
        classes.registerField("MenuControlData", "defaultValue", 0);
        classes.registerField("MenuControlData", "options", std::make_shared<ArrayValue>());
        classes.registerField("MenuControlData", "min", 0);
        classes.registerField("MenuControlData", "max", 100);
        classes.registerField("MenuControlData", "step", 1);
        classes.registerField("MenuControlData", "tooltip", std::string(""));
    }
}

REGISTER_CALLBACK(MenuData, LOICollection::server::Plugins::registerMenuDataClasses)
