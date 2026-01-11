#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class LanguagePlugin {
    public:
        ~LanguagePlugin();

        LanguagePlugin(LanguagePlugin const&) = delete;
        LanguagePlugin(LanguagePlugin&&) = delete;
        LanguagePlugin& operator=(LanguagePlugin const&) = delete;
        LanguagePlugin& operator=(LanguagePlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static LanguagePlugin& getInstance();

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI std::string getLanguageCode(Player& player);
        LOICOLLECTION_A_NDAPI std::string getLanguage(const std::string& mObject);
        LOICOLLECTION_A_NDAPI std::string getLanguage(Player& player);

        LOICOLLECTION_A_API   void set(Player& player, const std::string& langcode);

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        LanguagePlugin();

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