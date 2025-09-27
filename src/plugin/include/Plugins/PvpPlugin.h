#pragma once

#include <memory>

#include "base/Macro.h"

class Player;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class PvpPlugin {
    public:
        static PvpPlugin& getInstance() {
            static PvpPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void enable(Player& player, bool value);

        LOICOLLECTION_A_NDAPI bool isEnable(Player& player);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        PvpPlugin();
        ~PvpPlugin();

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