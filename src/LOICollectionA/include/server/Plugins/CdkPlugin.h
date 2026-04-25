#pragma once

#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/CdkGui.h"

class Player;
class JsonStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class CdkPlugin {
    public:
        ~CdkPlugin();

        CdkPlugin(CdkPlugin const&) = delete;
        CdkPlugin(CdkPlugin&&) = delete;
        CdkPlugin& operator=(CdkPlugin const&) = delete;
        CdkPlugin& operator=(CdkPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static CdkPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<JsonStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void create(const std::string& id, int time, bool personal);
        LOICOLLECTION_A_API   void remove(const std::string& id);

        LOICOLLECTION_A_API   void convert(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getCdks();

        LOICOLLECTION_A_NDAPI bool has(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        CdkPlugin();

        void registeryCommand();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<CdkGui> mGui;
    };
}