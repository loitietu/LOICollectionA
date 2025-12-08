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
    class NoticePlugin final {
    public:
        LOICOLLECTION_A_NDAPI static NoticePlugin& getInstance() {
            static NoticePlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI JsonStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const std::string& title, int priority, bool poiontout);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_NDAPI bool isClose(Player& player);
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
        NoticePlugin();
        ~NoticePlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class NoticePlugin::gui {
    private:
        NoticePlugin& mParent;

    public:
        gui(NoticePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void content(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentAdd(Player& player);
        LOICOLLECTION_A_API void contentRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void contentRemove(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void notice(Player& player);
        LOICOLLECTION_A_API void notice(Player& player, const std::string& id);
        LOICOLLECTION_A_API void open(Player& player);
    };
}