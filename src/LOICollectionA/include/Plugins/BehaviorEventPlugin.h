#pragma once

#include <memory>
#include <string>
#include <vector> 
#include <utility>
#include <functional>

#include "LOICollectionA/base/Macro.h"

class Vec3;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class BehaviorEventPlugin {
    public:
        struct Event;

    public:
        static BehaviorEventPlugin& getInstance() {
            static BehaviorEventPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI Event getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents();
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter = {});
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEventsByPosition(int dimension, std::function<bool(int x, int y, int z)> filter);
        LOICOLLECTION_A_API   std::vector<std::string> filter(std::vector<std::string> ids);

        LOICOLLECTION_A_API   void refresh();
        LOICOLLECTION_A_API   void write(const std::string& id, const Event& event);
        LOICOLLECTION_A_API   void back(const std::string& id);
        LOICOLLECTION_A_API   void clean(int hours);

        LOICOLLECTION_A_NDAPI bool isValid();
    
    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BehaviorEventPlugin();
        ~BehaviorEventPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    struct BehaviorEventPlugin::Event {
        std::string eventName;
        std::string eventTime;
        std::string eventType;

        int posX;
        int posY;
        int posZ;
        int dimension;

        std::vector<std::pair<std::string, std::string>> extendedFields;
    };
}