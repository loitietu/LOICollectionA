#pragma once

#include <memory>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class PvpPlugin {
    public:
        ~PvpPlugin();

        PvpPlugin(PvpPlugin const&) = delete;
        PvpPlugin(PvpPlugin&&) = delete;
        PvpPlugin& operator=(PvpPlugin const&) = delete;
        PvpPlugin& operator=(PvpPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static PvpPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void enable(Player& player, bool value);

        LOICOLLECTION_A_NDAPI bool isEnable(Player& player);
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
        PvpPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class PvpPlugin::gui {
    private:
        PvpPlugin& mParent;

    public:
        gui(PvpPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player);
    };
}