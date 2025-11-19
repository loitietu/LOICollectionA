#pragma once

#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "LOICollectionA/base/Macro.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    enum class MenuType {
        Simple,
        Modal,
        Custom
    };

    class MenuPlugin {
    public:
        LOICOLLECTION_A_NDAPI static MenuPlugin& getInstance() {
            static MenuPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI JsonStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const nlohmann::ordered_json& data);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);
        LOICOLLECTION_A_API   void handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        MenuPlugin();
        ~MenuPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;
        
        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class MenuPlugin::gui {
    private:
        MenuPlugin& mParent;

    public:
        gui(MenuPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void editNewInfo(Player& player, MenuType type);
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardCommand(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void custom(Player& player, const std::string& id);
        LOICOLLECTION_A_API void simple(Player& player, const std::string& id);
        LOICOLLECTION_A_API void modal(Player& player, const std::string& id);
        LOICOLLECTION_A_API void open(Player& player, const std::string& id);
    };
}
