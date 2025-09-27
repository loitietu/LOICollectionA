#pragma once

#include <memory>
#include <string>

#include "base/Macro.h"

class Player;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    enum class TransferType {
        online,
        offline
    };

    class WalletPlugin {
    public:
        static WalletPlugin& getInstance() {
            static WalletPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void transfer(const std::string& target, int score);
        LOICOLLECTION_A_API   void redenvelope(Player& player, const std::string& key, int score, int count);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        WalletPlugin();
        ~WalletPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class WalletPlugin::gui {
    private:
        WalletPlugin& mParent;

    public:
        gui(WalletPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void content(Player& player, const std::string& target, TransferType type);
        LOICOLLECTION_A_API void transfer(Player& player, TransferType type);
        LOICOLLECTION_A_API void redenvelope(Player& player);
        LOICOLLECTION_A_API void wealth(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}