#pragma once

#include <memory>
#include <string>
#include <vector> 
#include <utility>
#include <functional>

#include "LOICollectionA/base/Macro.h"

class Vec3;
class SQLiteStorage;

namespace ll {
    namespace event {
        class Event;
    }

    namespace io {
        class Logger;
    }
}

namespace LOICollection::Plugins {
    enum class BehaviorEventConfig {
        ModuleEnabled,
        RecordDatabase,
        OutputConsole
    };

    class BehaviorEventPlugin {
    public:
        struct Event;

        ~BehaviorEventPlugin();

        BehaviorEventPlugin(BehaviorEventPlugin const&) = delete;
        BehaviorEventPlugin(BehaviorEventPlugin&&) = delete;
        BehaviorEventPlugin& operator=(BehaviorEventPlugin const&) = delete;
        BehaviorEventPlugin& operator=(BehaviorEventPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static BehaviorEventPlugin& getInstance();

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI Event getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents(int limit = -1);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter = {}, int limit = -1);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getEventsByPosition(int dimension, std::function<bool(int x, int y, int z)> filter, int limit = -1);
        LOICOLLECTION_A_API   std::vector<std::string> filter(std::vector<std::string> ids);

        LOICOLLECTION_A_API   void write(const std::string& id, const Event& event);
        LOICOLLECTION_A_API   void back(std::vector<std::string>& ids);
        LOICOLLECTION_A_API   void clean(int hours);

        LOICOLLECTION_A_NDAPI bool isValid();
    
    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        BehaviorEventPlugin();

        template <typename T>
        void registeryEvent(
            const std::string& name,
            const std::string& type,
            const std::string& id,
            std::function<bool(BehaviorEventConfig)> config,
            std::function<void(ll::event::Event&, Event&)> process,
            std::function<std::string(std::string, ll::event::Event&)> formatter
        );

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