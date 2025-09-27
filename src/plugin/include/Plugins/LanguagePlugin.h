#pragma once

#include <memory>
#include <string>

#include "base/Macro.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class LanguagePlugin {
    public:
        static LanguagePlugin& getInstance() {
            static LanguagePlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI std::string getLanguageCode(Player& player);
        LOICOLLECTION_A_NDAPI std::string getLanguage(const std::string& mObject);
        LOICOLLECTION_A_NDAPI std::string getLanguage(Player& player);

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        LanguagePlugin();
        ~LanguagePlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class LanguagePlugin::gui {
    private:
        LanguagePlugin& mParent;

    public:
        gui(LanguagePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player);
    };
}