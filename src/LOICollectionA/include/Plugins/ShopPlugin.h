#pragma once

#include <string>
#include <functional>

#include <nlohmann/json_fwd.hpp>

#include "LOICollectionA/base/Macro.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    enum class ShopType {
        buy,
        sell
    };

    enum class AwardType {
        commodity,
        title,
        from
    };

    class ShopPlugin final {
    public:
        LOICOLLECTION_A_NDAPI static ShopPlugin& getInstance();

        LOICOLLECTION_A_NDAPI JsonStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const nlohmann::ordered_json& data);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void onShopCreate(std::function<void(const std::string&)> fn);
        LOICOLLECTION_A_API   void onShopRemove(std::function<void(const std::string&)> fn);

        LOICOLLECTION_A_API   void executeCommand(Player& player, std::string cmd);

        LOICOLLECTION_A_NDAPI bool checkModifiedData(Player& player, nlohmann::ordered_json data, int number);
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
        ShopPlugin();
        ~ShopPlugin();

        void registeryCommand();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class ShopPlugin::gui {
    private:
        ShopPlugin& mParent;

    public:
        gui(ShopPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void editNewInfo(Player& player, ShopType type);
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardNewInfo(Player& player, const std::string& id, ShopType type, AwardType awardType);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void menu(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void commodity(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void title(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void open(Player& player, const std::string& id);
    };
}