#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class CdkPlugin {
    public:
        ~CdkPlugin();

        CdkPlugin(CdkPlugin const&) = delete;
        CdkPlugin(CdkPlugin&&) = delete;
        CdkPlugin& operator=(CdkPlugin const&) = delete;
        CdkPlugin& operator=(CdkPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static CdkPlugin& getInstance();

        LOICOLLECTION_A_NDAPI JsonStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, int time, bool personal);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void convert(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI bool has(const std::string& id);
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
        CdkPlugin();

        void registeryCommand();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class CdkPlugin::gui {
    private:
        CdkPlugin& mParent;

    public:
        gui(CdkPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void convert(Player& player);
        LOICOLLECTION_A_API void cdkNew(Player& player);
        LOICOLLECTION_A_API void cdkRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkRemove(Player& player);
        LOICOLLECTION_A_API void cdkAwardScore(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardTitle(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAward(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}