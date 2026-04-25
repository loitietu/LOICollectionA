#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/NoticeGui.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class NoticePlugin {
    public:
        ~NoticePlugin();

        NoticePlugin(NoticePlugin const&) = delete;
        NoticePlugin(NoticePlugin&&) = delete;
        NoticePlugin& operator=(NoticePlugin const&) = delete;
        NoticePlugin& operator=(NoticePlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static NoticePlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, const std::string& title, int priority, bool poiontout);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void setClose(Player& player, bool enable);

        LOICOLLECTION_A_NDAPI bool has(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isClose(Player& player);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        NoticePlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<NoticeGui> mGui;
    };
}