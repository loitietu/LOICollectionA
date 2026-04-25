#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/ShopGui.h"
#include "LOICollectionA/include/server/Plugins/types/ShopType.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class ShopPlugin {
    public:
        ~ShopPlugin();

        ShopPlugin(ShopPlugin const&) = delete;
        ShopPlugin(ShopPlugin&&) = delete;
        ShopPlugin& operator=(ShopPlugin const&) = delete;
        ShopPlugin& operator=(ShopPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static ShopPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const nlohmann::ordered_json& data);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   bool commodity(Player& player, int number, const nlohmann::ordered_json& data, ShopType type);
        LOICOLLECTION_A_API   bool title(Player& player, const nlohmann::ordered_json& data, ShopType type);

        LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);

        LOICOLLECTION_A_NDAPI bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number);
        
        LOICOLLECTION_A_NDAPI bool has(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        ShopPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<ShopGui> mGui;
    };
}