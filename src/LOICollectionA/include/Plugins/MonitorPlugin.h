#pragma once

#include <memory>
#include <string>
#include <vector>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::Plugins {
    enum class SidebarType {
        Ascending = 0,
        Descending = 1
    };

    class MonitorPlugin {
    public:
        LOICOLLECTION_A_NDAPI static MonitorPlugin& getInstance();

        LOICOLLECTION_A_API   void addSidebar(Player& player, const std::string& id, const std::string& name, SidebarType type);
        LOICOLLECTION_A_API   void setSidebar(Player& player, const std::string& id, std::vector<std::pair<std::string, int>> data);
        LOICOLLECTION_A_API   void removeSidebar(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        MonitorPlugin();
        ~MonitorPlugin();

        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}