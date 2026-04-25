#pragma once

#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/WalletGui.h"

class Player;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class WalletPlugin {
    public:
        ~WalletPlugin();

        WalletPlugin(WalletPlugin const&) = delete;
        WalletPlugin(WalletPlugin&&) = delete;
        WalletPlugin& operator=(WalletPlugin const&) = delete;
        WalletPlugin& operator=(WalletPlugin&&) = delete;    

    public:
        LOICOLLECTION_A_NDAPI static WalletPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI std::string getPlayerInfo(const std::string& uuid);

        LOICOLLECTION_A_NDAPI std::vector<std::pair<std::string, std::string>> getPlayerInfo();

        LOICOLLECTION_A_API   bool forTransfer(Player& player, const std::string& target, const std::string& name, int score);

        LOICOLLECTION_A_API   void transfer(const std::string& target, int score);
        LOICOLLECTION_A_API   void wealth(Player& player);
        LOICOLLECTION_A_API   void redenvelope(Player& player, const std::string& key, int score, int count);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::string getTargetScoreboard();

        LOICOLLECTION_A_NDAPI double getExchangeRate();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        WalletPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct RedEnvelopeEntry;

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<WalletGui> mGui;
    };
}