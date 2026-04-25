#pragma once

#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/MenuGui.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class MenuPlugin {
    public:
        ~MenuPlugin();

        MenuPlugin(MenuPlugin const&) = delete;
        MenuPlugin(MenuPlugin&&) = delete;
        MenuPlugin& operator=(MenuPlugin const&) = delete;
        MenuPlugin& operator=(MenuPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static MenuPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const nlohmann::ordered_json& data);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);
        LOICOLLECTION_A_API   void handleAction(Player& player, const nlohmann::ordered_json& action, const nlohmann::ordered_json& original);

        LOICOLLECTION_A_NDAPI bool has(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        MenuPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;
        
        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<MenuGui> mGui;
    };
}
